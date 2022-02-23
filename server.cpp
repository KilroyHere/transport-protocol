#include <string>
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
    std::string packet(packetBuffer);

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

      //reset its timer
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
  using namespace std;
  if (p == nullptr) return false; 
  vector<TCPPacket*>& packetBuffer = m_connectionIdToBuffer[connId];
  int& nextExpectedSeqNum = m_connectionExpectedSeqNums[connId];


  int seqNum = p->getSeqNum();
  int currentSeqNum = nextExpectedSeqNum;
  for(int i=0; i < packetBuffer.size(); i++)
  {
    if (currentSeqNum == seqNum)
    {
      if (packetBuffer[i]==nullptr) // the slot was empty
      {
        packetBuffer[i] = p;
        return true; // successful addition to the buffer
      }
      else
        return false;
    }
    currentSeqNum += MAX_PAYLOAD_LENGTH; // sequence number would increase by this number except for the last packet
  }
  return false;
}

/**
 * @brief Writes consecutive packets that have arrived in the buffer from the start, updates next expected sequence number, and initializes next slots in buffer to nullptr
 * 
 * @return number of packets that were written
 */
int Server::flushBuffer(int connId)
{
  using namespace std;
  vector<TCPPacket*>& packetBuffer = m_connectionIdToBuffer[connId];
  int& nextExpectedSeqNum = m_connectionExpectedSeqNums[connId];

  if (packetBuffer.size() < 1) return -1; // shouldn't even be a thing
  int numFlushed = 0;
  while (packetBuffer[0] != nullptr)
  {
    writeToFile(packetBuffer[0]->getPayload());
    nextExpectedSeqNum += packetBuffer[0]->getSeqNum() + packetBuffer[0]->getPayloadLength();
    delete packetBuffer[0]; // call the destructor on TCPPacket object
    packetBuffer.erase(packetBuffer.begin()); // remove the first element
    packetBuffer.push_back(nullptr); // window advanced to the next packet
    numFlushed++;
  }
  return numFlushed;
}

int main()
{
  std::cerr << "server is not implemented yet" << std::endl;
}
