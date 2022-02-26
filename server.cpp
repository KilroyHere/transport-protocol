#include <string>
#include <bitset>
#include <thread>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>
#include <fcntl.h>
#include <chrono>
#include <ctime>
#include "server.hpp"
#include "constants.cpp"
#include "utilities.cpp"
#include "tcp.cpp"

// SERVER IMPLEMENTATION

/**
 * @brief Enumerate through timers and if any timer has timed out, release resources for that connection
 * 
 */
void Server::closeTimedOutConnectionsAndRetransmitFIN()
{
  //TODO: replace this with final implementation of timer hashmap
  for (auto it = m_connectionIdToTCB.begin(); it != m_connectionIdToTCB.end(); it++)
  {
    if (!checkTimer(it->first, 10)) // check timer by connection id
    {                           // timer run out
      closeConnection(it->first);
    }

    if (it->second->connectionState == FIN_RECEIVED && !checkTimer(it->first, 0.5)) 
    {                           
      sendPacket(&clientInfo, clientInfoLen, it->second->finPacket);
      setTimer(it->first);
    }
  }
}

/**
 * @brief Shift the window a certain number of bytes ahead for a connection
 * 
 * @param id connection id 
 * @param bytes number of bytes to shift winodw by
 */
void Server::moveWindow(int connId, int bytes)
{
  using namespace std;
  vector<char> &connectionBuffer = m_connectionIdToTCB[connId]->connectionBuffer;
  bitset<RWND_BYTES> &connectionBitset = m_connectionIdToTCB[connId]->connectionBitvector;

  if (bytes >= connectionBuffer.size())
    return;

  for (int i = 0; i < RWND_BYTES - bytes; i++) // last value of i will be RWND_BYTES - bytesToWriite - 1
  {
    connectionBuffer[i] = connectionBuffer[i + bytes];
    connectionBitset[i] = connectionBitset[i + bytes];
  }

  for (int i = RWND_BYTES - bytes; i < RWND_BYTES; i++)
  {
    connectionBuffer[i] = 0;
    connectionBitset[i] = 0;
  }
}

/**
 * @brief Function that handles all of the incoming connections
 * 
 */
void Server::handleConnection()
{
  using namespace std;
  while (true) // since server will run indefinitely, and we're not using multithreading/forking
  {
    closeTimedOutConnectionsAndRetransmitFIN(); // check and close any timed out connection every iteration
    // prepare to read incoming packet
    char packetBuffer[MAX_PACKET_LENGTH + 1]; // last byte nullbyte
    sockaddr clientInfo;                      // needed to send response
    socklen_t clientInfoLen = sizeof(clientInfo);
    int bytesRead = recvfrom(m_sockFd, packetBuffer, MAX_PACKET_LENGTH, 0, &clientInfo, &clientInfoLen);
    packetBuffer[MAX_PACKET_LENGTH] = 0; // mark the end with a null byte

    // convert C string to std::string
    std::string packet = convertCStringtoStandardString(packetBuffer, bytesRead);

    TCPPacket *p = new TCPPacket(packet); // create new packet from string
    int packetConnId = p->getConnId();    // get the connection ID of the packet

    /*
    addNewConnection(p);
    handleFin(p);
    if (payload)
      processing further potentially based on state
    */

    addNewConnection(p, clientInfo, clientInfoLen);
    handleFin(p);
    if (m_connectionIdToTCB.find(packetConnId) == m_connectionIdToBuffer.end())
    { 
      // there should be no other reason for us to find a SYN unless we're not adding a new connection
      // do nothing, packet will be deleted
    }
    else
    {
      setTimer(packetConnId);
      addPacketToBuffer(packetConnId, p);
      flushBuffer(packetConnId);

      delete p;
      p = nullptr;

      // check if the a SYN-ACK needs to be sent 
      bool synFlag = false;
      if(m_connectionIdToTCB[packetConnId]->connectionState == ACK_AWAITING)
      {
        synFlag = true;
      }

      // if reached this block, then packet was valid and ACK should be sent
      TCPPacket *ackPacket = new TCPPacket(
          m_connectionIdToTCB[packetConnId]->connectionServerSeqNum,   // sequence number
          m_connectionIdToTCB[packetConnId]->connectionExpectedSeqNum, // ack number
          packetConnId,                                                // connection id
          true,                                                        // is an ACK
          synFlag,                                                       // decided by synFlag
          false,                                                       // is not FIN
          0,                                                           // no payload
          "");
      sendPacket(&clientInfo, clientInfoLen, ackPacket);
      delete ackPacket;
      ackPacket = nullptr;
    }
  }
}

/**
 * @brief Adds packet to the buffer
 * 
 * @param p pointer to the TCPPacket
 * @return True if packet was successfully added to the buffer, false if no space was available,the pointer was nullptr, or it was a duplicate
 */
bool Server::addPacketToBuffer(int connId, TCPPacket *p)
{
  /*
Several implementations could have been used here in the case that we 
receive a packet that has fills a gap AND also overwrites on either side.
One approach would be that if any byte in the range of the packet in the window 
has already been written to then drop the packet, removing the chance of an overwrite.
However, the counter argument to that was -- what's the guarantee that the first packet
I got was not corrupted (since such a case would only arise in the case of a corruption)

Thus, the current implementation is that if there is an overlap in bits, overwrite.

Packets that have overlapping bytes should result in undefined behavior, which is 
exactly what this implementation would provide, since the order of arrival of the 
packets is not definite, and the result is therefore indeterminate.
*/

  using namespace std;
  if (p == nullptr)
    return false;
  vector<char> &connectionBuffer = m_connectionIdToTCB[connId]->connectionBuffer;
  bitset<RWND_BYTES> &connectionBitset = m_connectionIdToTCB[connId]->connectionBitvector;
  int nextExpectedSeqNum = m_connectionIdToTCB[connId]->connectionExpectedSeqNum;

  int packetSeqNum = p->getSeqNum();
  int payloadLen = p->getPayloadLength();
  char *payloadBuffer = p->getPayload();

  if (nextExpectedSeqNum + RWND_BYTES < packetSeqNum + payloadLen)
    return false; // runs out of bounds
  if (packetSeqNum < nextExpectedSeqNum)
    return false; // runs before bounds
  if (m_connectionIdToTCB[connId]->connectionState == FIN_RECEIVED)
    return false;

  for (int i = 0; i < payloadLen; i++)
  {
    // TODO: may need to use the sequence number reset wrap around value eventually with modulus
    connectionBuffer[i + packetSeqNum - nextExpectedSeqNum] = payloadBuffer[i];
    connectionBitset[i + packetSeqNum - nextExpectedSeqNum] = 1; // now mark as used, regardless of overwrite
  }
  return true;
}

/**
 * @brief Writes consecutive packets that have arrived in the buffer from the start, updates next expected sequence number, and initializes next slots in buffer to nullptr
 * 
 * @return number of bytes that were written
 */
int Server::flushBuffer(int connId)
{
  using namespace std;
  vector<char> &connectionBuffer = m_connectionIdToBuffer[connId]->connectionBuffer;
  bitset<RWND_BYTES> &connectionBitset = m_connectionIdToBuffer[connId]->connectionBitVector;
  int &nextExpectedSeqNum = m_connectionIdToBuffer[connId]->connectionExpectedSeqNum;

  // find the number of bytes to write
  int bytesToWrite = 0;
  for (; bytesToWrite < RWND_BYTES && connectionBitset[bytesToWrite] == 1; bytesToWrite++)
    ;

  char *outputBuffer = new char[bytesToWrite + 1]; 

  std::copy(connectionBuffer.begin(), connectionBuffer.begin() + bytesToWrite, outputBuffer);
  outputBuffer[bytesToWrite] = 0; // just for safety
  
  writeToFile(connId, outputBuffer, bytesToWrite);
  nextExpectedSeqNum += bytesToWrite; // update the next expected sequence number

  delete outputBuffer;
  outputBuffer = nullptr;

  moveWindow(connId, bytesToWrite);

  return bytesToWrite;
}

/**
 * @brief 
 * 
 * @return bool
 */
bool addNewConnection(TCPPacket *p, sockaddr *clientInfo, socklen_t clientInfoLen)
{
  if (p->isSYN() && p->getConnId() == 0) // new connection id
  {
    // Get the connection ID 
    int packetConnId = nextAvailableConnectionId;

    // create an output file 
    // TODO: assumed existance of save directory
    string pathName = "~/save/" + std::to_string(packetConnId) + ".file";
    int fd = open(pathName.c_str(), O_CREAT);

    // set up TCB and start timer
    m_connectionIdToTCB[packetConnId] = new TCB(p->getSeqNum() + 1, fd, AWAITING_ACK, true);
    m_connectionIdToTCB[packetConnId]->clientInfo = clientInfo;
    m_connectionIdToTCB[packetConnId]->clientInfoLen = clientInfoLen;
    startTimer(packetConnId);

    ++nextAvailableConnectionId; //update the next available connection Id
    return true;
  }

  // TODO: Make connection state an enum

  // Update Connection state in case of an ACK 
  else if (p->isACK() && m_connectionIdToTCB[p->getConnId()]->connectionState == AWAITING_ACK) // new connection id
  {
    m_connectionIdToTCB[p->getConnId()]->connectionState = CONNECTION_ESTABLISHED;
    return true;
  }

  else
  {
    return false;
  }
}

/**
 * @brief 
 */
void closeConnection(int connId)
{
  // delete TCB Block 
  delete m_connectionIdToTCB[p->getConnId()];
  m_connectionIdToTCB[p->getConnId()] = nullptr;

  // remove entry 
  m_connectionIdToTCB.erase ( m_connectionIdToTCB.find(p->getConnId()), m_connectionIdToTCB.end() );
}

void setTimer(int connId)
{
  m_connectionIdToTCB[connId]->connectionTimer = std::chrono::system_clock::now();
}

bool checkTimer(int connId, float timerLimit)
{
  c_time current_time = std::chrono::system_clock::now();
  c_time start_time = m_connectionIdToTCB[connId]->connectionTimer;
  std::chrono::duration<double> elapsed_time = current_time - start_time;
  int elapsed_seconds = elapsed_time.count();
  
  return (elapsed_seconds >= timerLimit);
}

void handleFin(TCPPacket *p)
{
  if(p->isFIN())
  {
    // change state to FIN_RECEIVED -> wait for ACK for FIN-ACK
    m_connectionIdToTCB[connId]->connectionState = FIN_RECEIVED;
    ++m_connectionIdToTCB[p->getConnId()]->nextExpectedSeqNum; // updating expected sequence number by 1

    TCPPacket *finPacket = new TCPPacket(
          m_connectionIdToTCB[packetConnId]->connectionServerSeqNum,   // sequence number
          m_connectionIdToTCB[packetConnId]->connectionExpectedSeqNum, // ack number
          packetConnId,                                                // connection id
          true,                                                        // is ACK
          false,                                                       // is not SYN
          true,                                                       // is FIN
          0,                                                           // no payload
          "");
    sendPacket(&clientInfo, clientInfoLen, finPacket);

    // save the FIN packet
    m_connectionIdToTCB[connId]->finPacket = finPacket;
    setTimer(p->getConnId()); // set timer 
  }

  else if(p->isACK() && m_connectionIdToTCB[p->getConnId()]->connectionState == FIN_RECEIVED)
  {
    delete m_connectionIdToTCB[p->getConnId()]->finPacket;
    m_connectionIdToTCB[p->getConnId()]->finPacket = nullptr;
    closeConnection(p->getConnId());
  }
}

	int Server::outputToStdout(std::string message)
  {
    cout << message;
  }
	int Server::outputToStderr(std::string message)
  {
    cerr << message;
  }

  void Server::sendPacket(sockaddr *clientInfo, int clientInfoLen, TCPPacket *p)
  {

  }

int main()
{
  std::cerr << "server is not implemented yet" << std::endl;
}
