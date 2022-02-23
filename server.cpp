#include <string>
#include <bitset>
#include <thread>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>
#include "server.hpp"
#include "constants.cpp"

// SERVER IMPLEMENTATION 
/**
 * @brief Function that handles all of the incoming connections
 * 
 */
void Server::handleConnection()
{ 
  using namespace std;
  while (true) // since server will run indefinitely, and we're not using multithreading/forking
  {

    //TODO: replace this with final implementation of timer hashmap
    for(std::unordered_map<int, time_t>::iterator it = m_connectionTimer.begin(); it!=m_connectionTimer.end(); it++)
    {
      if (!checkTimer(it->first)) // check timer by connection id
      {  // timer run out
        closeConnection(it->first);
      }
    }

    // prepare to read incoming packet
    char packetBuffer[MAX_PACKET_LENGTH+1]; // last byte nullbyte
    sockaddr clientInfo; // needed to send response
    socklen_t clientInfoLen = sizeof(clientInfo);
    int bytesRead = recvfrom(m_sockFd, packetBuffer, MAX_PACKET_LENGTH, 0, &clientInfo, &clientInfoLen);
    packetBuffer[MAX_PACKET_LENGTH] = 0; // mark the end with a null byte 

    // convert C string to std::string
    std::string packet;
    for (int i = 0; i < bytesRead; i++) // need the for loop to force null byte in the payload to the packet
    {
      packet += packetBuffer[i];
    }

    TCPPacket* p = new TCPPacket(packet); // create new packet from string
    int packetConnId = p->getConnId(); // get the connection ID of the packet


    if (p->isSYN() && packetConnId == 0)
      addNewConnection(p);
    else if (m_connectionIdToBuffer.find(packetConnId) == m_connectionIdToBuffer.end() || p->isSYN())
    { // there should be no other reason for us to find a SYN unless we're not adding a new connection
      delete p; // discard the packet
      p = nullptr; // safety is our #1 priority :) 
    }
    else if (p->isFIN())
    {
      handleFin(p);
    }
    else
    {
      // packet received in a valid connection
      //reset connection timer
      resetTimer(packetConnId);

      if (!addPacketToBuffer(packetConnId, p)) // no change to the buffer
      {
        delete p; // discard the packet
        p = nullptr;
      }
      else
      {
        flushBuffer(packetConnId); // packets written to file and next expected sequence number automatically updated
      }
      // if reached this block, then packet was valid and ACK should be sent
      TCPPacket* ackPacket = new TCPPacket(
        m_connectionServerSeqNums[packetConnId],      // sequence number 
        m_connectionExpectedSeqNums[packetConnId],    // ack number 
        packetConnId,                                 // connection id 
        true,                                         // is an ACK
        false,                                        // not a SYN
        false,                                        // not a FIN
        0,                                            // no payload
        ""
      );
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
bool Server::addPacketToBuffer(int connId, TCPPacket* p)
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
  if (p == nullptr) return false; 
  vector<char>& connectionBuffer = m_connectionIdToBuffer[connId];
  bitset<RWND_BYTES>& connectionBitset = m_connectionBitvector[connId];
  int nextExpectedSeqNum = m_connectionExpectedSeqNums[connId];

  int packetSeqNum = p->getSeqNum();
  int payloadLen = p->getPayloadLength();
  char* payloadBuffer = p->getPayload();

  if (nextExpectedSeqNum + RWND_BYTES < packetSeqNum + payloadLen) return false; // runs out of bounds
  if (packetSeqNum < nextExpectedSeqNum) return false; // runs before bounds

  for (int i=0; i < payloadLen; i++)
  {
    // TODO: may need to use the sequence number reset wrap around value eventually with modulus 
    connectionBuffer[i + packetSeqNum - nextExpectedSeqNum] = payloadBuffer[i];
    connectionBitset[i + packetSeqNum - nextExpectedSeqNum] = 1; // now mark as used, regardless of overwrite
  }
}

/**
 * @brief Writes consecutive packets that have arrived in the buffer from the start, updates next expected sequence number, and initializes next slots in buffer to nullptr
 * 
 * @return number of bytes that were written
 */
int Server::flushBuffer(int connId)
{
  using namespace std;
  vector<char>& connectionBuffer = m_connectionIdToBuffer[connId];
  bitset<RWND_BYTES>& connectionBitset = m_connectionBitvector[connId];
  int& nextExpectedSeqNum = m_connectionExpectedSeqNums[connId];

  // find the number of bytes to write
  int bytesToWrite = 0;
  for (; bytesToWrite < RWND_BYTES; bytesToWrite++)
    if (connectionBitset[bytesToWrite]==0)
      break;
  
  char* outputBuffer = new char[bytesToWrite+1];
  std::copy(connectionBuffer.begin(), connectionBuffer.begin() + bytesToWrite, outputBuffer);
  outputBuffer[bytesToWrite] = 0; // just for safety

  writeToFile(outputBuffer, bytesToWrite);
  nextExpectedSeqNum += bytesToWrite;     // update the next expected sequence number 

  delete outputBuffer;
  outputBuffer = nullptr;

  for(int i=0; i< RWND_BYTES - bytesToWrite; i++) // last value of i will be RWND_BYTES - bytesToWriite - 1 
  {
    connectionBuffer[i] = connectionBuffer[i + bytesToWrite];
    connectionBitset[i] = connectionBitset[i + bytesToWrite];
  }

  for (int i= RWND_BYTES - bytesToWrite; i < RWND_BYTES; i++)
  {
    connectionBuffer[i] = 0;
    connectionBitset[i] = 0;
  }

  return bytesToWrite;
}

int main()
{
  std::cerr << "server is not implemented yet" << std::endl;
}
