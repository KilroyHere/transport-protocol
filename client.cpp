#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <vector>
#include <chrono>
#include <iostream>
#include <errno.h>
#include <string.h>
#include "constants.hpp"
#include "tcp.hpp"
#include "client.hpp"
#include "utilities.cpp"

// need to set up a non block recv api
Client::Client(std::string hostname, std::string port, std::string fileName)
{
  struct addrinfo hints, *servInfo, *p;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // set to AF_INET to use IPv4
  hints.ai_socktype = SOCK_DGRAM;

  m_blseek = 0;
  m_flseek = 0;
  m_largestSeqNum = 0;
  m_relSeqNum = 0;

  int ret;
  if ((ret = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &servInfo)) != 0)
  {
    perror("getaddrinfo");
    exit(1);
  }
  int sockfd = -1;

  for (p = servInfo; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      perror("talker: socket");
      continue;
    }
    break;
  }
  // non blocking receiving
  int r = fcntl(sockfd, F_SETFL, O_NONBLOCK);

  if (p == NULL)
  {
    perror("talker: failed to create socket");
    exit(1);
  }

  m_serverInfo = *p;
  freeaddrinfo(servInfo); // free the next, keep the current;
  servInfo = NULL;
}


/**
 * @brief Changes the values of CWND. Should be called when 1 ACK is received. 
 * Should also be careful about duplicate ACKs to ensure they do not affect congestionControl
 * 
 * @return the number of bytes congestion control has shifted forward
 */
int Client::congestionControl()
{
  // one ACK received. Update the congestion control
/*
  (Slow start)            If CWND < SS-THRESH: CWND += 512
  (Congestion Avoidance)  If CWND >= SS-THRESH: CWND += (512 * 512) / CWND
*/
  int newCwnd = m_cwnd;
  if (m_cwnd == MAX_CWND_BYTES)
  {
    newCwnd = m_cwnd;
  }
  if (m_cwnd < m_ssthresh) 
  {
    newCwnd += MAX_PAYLOAD_LENGTH;
  }
  else 
  {
    // integer division is the same as floor division for positive values
    newCwnd = (MAX_PAYLOAD_LENGTH * MAX_PAYLOAD_LENGTH) / m_cwnd; 
  }
  int diff = newCwnd - m_cwnd;
  m_cwnd = newCwnd;
  return diff;
}

/**
 * @brief Shift all resources relating to the current window forward for all inorder ACKs received from the start
 * 
 * NOTE: This does not take any responsibility of moving the byte buffer itself forward
 * 
 * @return Number of payload bytes that have been shifted forward 
 */
int Client::shiftWindow()
{
  int shiftedIndices = 0;
  int shiftedBytes = 0;

  for (int i=0; i<m_packetBuffer.size(); i++)
  {
    if (! m_packetACK[i]) break;
    shiftedIndices++;
    shiftedBytes += m_packetBuffer[i]->getPayloadLength();
  }

  // shift the values ahead
  for (int i=shiftedIndices; i < m_packetBuffer.size(); i++)
  {
    delete m_packetBuffer[i - shiftedIndices];
    m_packetBuffer[i - shiftedIndices] = nullptr;
    m_packetBuffer[i - shiftedIndices] = m_packetBuffer[i];
    m_packetACK[i - shiftedIndices] = m_packetACK[i];
    m_packetTimers[i - shiftedIndices] = m_packetTimers[i];
    m_sentOnce[i - shiftedIndices] = m_sentOnce[i];
  }

  // initialize the values at the end
  for (int i= m_packetBuffer.size() - shiftedIndices; i < m_packetBuffer.size(); i++)
  {
    m_packetBuffer[i] = nullptr; 
    m_packetACK[i] = false;
    // nothing to do for the timers 
    m_sentOnce[i] = false;
  }

  m_blseek += shiftedBytes;
  m_relSeqNum += shiftedBytes;
  m_relSeqNum %= MAX_SEQ_NUM + 1;

  return shiftedBytes;
}

void Client::markAck(TCPPacket* p)
{
  using namespace std;
  if (p == nullptr) 
  {
    cerr << "Unexpected nullptr found in Client::markAck" << endl;
    return;
  }
  int ack = p->getAckNum();

  // currently no implementation of what to do when ACK is beyond the window
  // since it is not clear which function to bring that into

  for (int i=0; i<m_packetBuffer.size(); i++)
  {
    if (ack < m_packetBuffer[i]->getSeqNum())
      break;
    else
      m_packetACK[i] = true;
  }
}

/**
 * @brief Gives pointer to packet to read, if available in the socket
 * 
 * @return Pointer to TCPPacket struct which the caller must delete to free the memory. If 
 * If socket is empty, return nullptr.
 */
TCPPacket* Client::recvPacket()
{
  char buffer[MAX_PACKET_LENGTH];

  int bytes = recvfrom(m_sockFd, buffer, MAX_PACKET_LENGTH, 0, NULL, 0); // Already have the address info of the server

  // nothing was available to read at the socket, so no new packet arrived
  if (bytes == -1) return nullptr;

  std::string stringBuffer = convertCStringtoStandardString(buffer, MAX_PACKET_LENGTH);
  TCPPacket* p = new TCPPacket(stringBuffer);
  return p;
}

int main()
{
  std::cerr << "client is not implemented yet" << std::endl;
}
