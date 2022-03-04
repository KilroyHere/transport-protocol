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

  for (int i = 0; i < m_packetBuffer.size(); i++)
  {
    if (!m_packetACK[i])
      break;
    shiftedIndices++;
    shiftedBytes += m_packetBuffer[i]->getPayloadLength();
  }

  // shift the values ahead
  for (int i = shiftedIndices; i < m_packetBuffer.size(); i++)
  {
    delete m_packetBuffer[i - shiftedIndices];
    m_packetBuffer[i - shiftedIndices] = nullptr;
    m_packetBuffer[i - shiftedIndices] = m_packetBuffer[i];
    m_packetACK[i - shiftedIndices] = m_packetACK[i];
    m_packetTimers[i - shiftedIndices] = m_packetTimers[i];
    m_sentOnce[i - shiftedIndices] = m_sentOnce[i];
  }

  // initialize the values at the end
  for (int i = m_packetBuffer.size() - shiftedIndices; i < m_packetBuffer.size(); i++)
  {
    m_packetBuffer[i] = nullptr;
    m_packetACK[i] = false;
    // nothing to do for the timers
    m_sentOnce[i] = false;
  }

  // TODO: recheck this line based on final implementation
  m_lseek += shiftedBytes;

  return shiftedBytes;
}

void Client::markAck(TCPPacket *p)
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

  for (int i = 0; i < m_packetBuffer.size(); i++)
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
TCPPacket *Client::recvPacket()
{
  char buffer[MAX_PACKET_LENGTH];

  int bytes = recvfrom(m_sockFd, buffer, MAX_PACKET_LENGTH, 0, NULL, 0); // Already have the address info of the server

  // nothing was available to read at the socket, so no new packet arrived
  if (bytes == -1)
    return nullptr;

  std::string stringBuffer = convertCStringtoStandardString(buffer, MAX_PACKET_LENGTH);
  TCPPacket *p = new TCPPacket(stringBuffer);
  return p;
}

bool Client::verifySynAck(TCPPacket *synAckPacket)
{
  return synAckPacket->getSeqNum() == INIT_SERVER_SEQ_NUM &&
         synAckPacket->getAckNum() == INIT_CLIENT_SEQ_NUM + 1 &&
         synAckPacket->isACK() == true &&
         synAckPacket->isSYN() == true &&
         synAckPacket->isFIN() == false;
}

bool Client::verifyFinAck(TCPPacket *finAckPacket)
{
  return finAckPacket->getSeqNum() == m_ackNumber &&
         finAckPacket->getAckNum() == m_sequenceNumber &&
         finAckPacket->isACK() == true &&
         finAckPacket->isSYN() == false;
  // not verifing for fin as there are 2 cases
}

/**
 * @brief This performs a 2 way handshake (i.e Client sends a syn packet to the server and server sends
 * back a syn-ack packet). The Client will then set the ack flag on for the first packet sent with payload
 * to the server. This function is blocking and will not return until connection is established. 
 * 
 * It will set a timer for 10s and if no reply comes from the server it will also call closeConnection()
 * 
 * If this function returns then this means that a connection is established with the client. 
 */
void Client::handshake()
{
  // Create the SYN packet
  TCPPacket *synPacket = new TCPPacket(
      m_sequenceNumber, // sequence number
      m_ackNumber,      // ack number
      m_connectionId,   // connection id
      false,            // is not an ACK
      true,             // is SYN
      false,            // is not FIN
      0,                // no payload
      "");

  // send the packet to server
  sendPacket(synPacket);
  m_sequenceNumber = (m_sequenceNumber + 1) % (MAX_SEQ_NUM + 1); // as syn is 1 byte

  setTimer(CONNECTION_TIMER); // set the connection timer as the first packet
  setTimer(SYN_PACKET_TIMER); // set the syn packet timer

  TCPPacket *synAckPacket;
  while (true)
  {
    synAckPacket = recvPacket(); // recv the synAck packet

    // if packet received and is in good form then break from loop
    if (synAckPacket != nullptr && verifySynAck(synAckPacket))
    {
      // reset connection timer as packet received
      setTimer(CONNECTION_TIMER);
      break; // received valid syn-ack packet -> now process it
    }

    // if connection timeout -> close connection
    if (!checkTimer(CONNECTION_TIMER, CONNECTION_TIMEOUT))
    {
      // delete syn and synAck ptrs
      // NOTE: synAck doesn't need to be deleted except in the rare case
      // when I receive an ill formed synAck packet
      delete synPacket;
      synPacket = nullptr;
      delete synAckPacket;
      synAckPacket = nullptr;

      closeConnection();
      return;
    }

    // if connection timeout -> close connection
    if (!checkTimer(SYN_PACKET_TIMER, RETRANSMISSION_TIMER))
    {
      // send packets and reset timers
      sendPacket(synPacket);
      setTimer(SYN_PACKET_TIMER);
    }
  }

  /* 
    if we reach here then this means that client has received
    the syn ack packet in valid form

    this implies that the work of handshake is done and now the 
    first packet can be sent with payload.
  */

  // set connection Id and ack no.
  m_ackNumber = (synAckPacket->getSeqNum() + 1) % (MAX_SEQ_NUM + 1); // +1 as SYN-ACK packet is 1 byte
  m_connectionId = synAckPacket->getConnId();

  // delete syn and syn-ack packets
  delete synPacket;
  synPacket = nullptr;
  delete synAckPacket;
  synAckPacket = nullptr;
}

/**
 * @brief This performs a 4 way handwave at the end to gracefully
 * close the connection. This function is called when the entire 
 * file is read. 
 * 
 * this will be called in run
 */
void Client::handwave()
{
  // send a fin packet
  // NOTE: I am assuming that m_sequence here is the next available
  // sequence number that I can send in the fin packet (so I won't
  // increment it by 1)
  // Create the FIN packet
  TCPPacket *finPacket = new TCPPacket(
      m_sequenceNumber, // sequence number
      0,                // ack number is 0 as ack flag is not set
      m_connectionId,   // connection id
      false,            // is not an ACK
      false,            // is not SYN
      true,             // is FIN
      0,                // no payload
      "");

  m_sequenceNumber = (m_sequenceNumber + 1) % (MAX_SEQ_NUM + 1);

  // send the packet to server
  sendPacket(finPacket);
  setTimer(FIN_PACKET_TIMER); // set the fin packet timer

  // NOTE: We need to handle both the cases when server
  // just sends ack or sends fin-ack combined
  TCPPacket *ackPacket;
  while (true)
  {
    ackPacket = recvPacket(); // recv the ack/fin-ack packet

    // if packet received and is in good form then break from loop
    if (ackPacket != nullptr && verifyFinAck(ackPacket))
    {
      // reset connection timer as packet received
      setTimer(CONNECTION_TIMER);

      // set the 2 sec timer at the end
      setTimer(FIN_END_TIMER);

      // update seq no. and ack no.
      m_ackNumber = (ackPacket->getSeqNum() + 1) % (MAX_SEQ_NUM + 1); // +1 as ACK packet is 1 byte
      m_sequenceNumber = ackPacket->getAckNum();                      // set the sequence no. for the next ack packet to be sent by client (NOT NEEDED JUST ASSURANCE)
      break;                                                          // received valid ack packet -> now process it
    }

    // if connection timeout -> close connection
    if (!checkTimer(CONNECTION_TIMER, CONNECTION_TIMEOUT))
    {
      // delete syn and synAck ptrs
      // NOTE: synAck doesn't need to be deleted except in the rare case
      // when I receive an ill formed synAck packet
      delete finPacket;
      finPacket = nullptr;
      delete ackPacket;
      ackPacket = nullptr;

      closeConnection();
      return;
    }

    // if connection timeout -> close connection
    if (!checkTimer(FIN_PACKET_TIMER, RETRANSMISSION_TIMER))
    {
      // send packets and reset timers
      sendPacket(finPacket);
      setTimer(FIN_PACKET_TIMER);
    }
  }

  // if reach here then ack or fin ack recevied

  // if fin not received -> receive fin
  /*
    On piazza it says that for every Fin recieved in the 2s, 
    client sends an ack 
  */
  bool serverFinRevd = ackPacket->isFIN();

  // create ACK PACKET
  TCPPacket *clientAckPacket = new TCPPacket(
      m_sequenceNumber, // sequence number
      m_ackNumber,      // ack number set as this is an ack packet
      m_connectionId,   // connection id
      true,             // is an ACK
      false,            // is not SYN
      false,            // is not FIN
      0,                // no payload
      "");

  if (serverFinRevd)
  {
    // send ack packet
    sendPacket(clientAckPacket);
    // NOTE: NO retransmission timers need to set
    // we only retransmit if we recieve a fin
  }

  TCPPacket *serverFinPacket;
  while (checkTimer(FIN_END_TIMER, CLIENT_CONNECTION_END_TIMER))
  {
    serverFinPacket = recvPacket(); // recv the fin packet

    // if packet received and is in good form then break from loop
    if (serverFinPacket != nullptr && serverFinPacket->isFIN())
    {
      sendPacket(clientAckPacket);
      delete serverFinPacket;
      serverFinPacket == nullptr;
    }
  }

  // if we reach here close connection and free memory
  delete finPacket;
  finPacket = nullptr;
  delete ackPacket;
  ackPacket = nullptr;
  delete clientAckPacket;
  clientAckPacket = nullptr;
  delete serverFinPacket;
  serverFinPacket = nullptr;

  closeConnection();
  return;
}

/**
 * @brief This function adds the provided packets to buffer and updates all other vectors to
 * have the default value corresponding to every new packets added 
 * 
 * @param packets the vector of packets to be added 
 */
void Client::addToBuffers(std::vector<TCPPacket *> packets)
{
  for (const auto &packet : packets)
  {
    m_packetBuffer.push_back(packet);
    m_packetACK.push_back(false);
    m_packetTimers.push_back(std::chrono::system_clock::now()); // setting to now as no default value of c_time
    m_sentOnce.push_back(false);
  }
}

/**
 * @brief This functions send all the packets that haven't been sent even once to the server 
 * It utilizes the bool values in m_sentOnce buffers to send the packets that haven't been sent even once
 * 
 * The function then determines if a packet is a dup or not
 */
void Client::sendPackets()
{
  /* Assuming all 4 vectors are always of the same size */
  for (int i = 0; i < m_sentOnce.size(); i++)
  {
    // We send the packets which are marked as false in sentOnce
    if (!m_sentOnce[i])
    {
      sendPacket(m_packetBuffer[i]);                        // send packets
      m_sentOnce[i] = true;                                 // set sentOnce to true
      m_packetTimers[i] = std::chrono::system_clock::now(); // start timer

      bool isDuplicate = isDup(m_packetBuffer[i]); // check if the packet is a duplicate packet;

      // print to stdout
    }
  }
}

bool Client::isDup(TCPPacket *p)
{
  int seqNum = p->getSeqNum();
  if (m_largestSeqNum < m_relSeqNum)
  {
    return (seqNum <= m_sequenceNumber) || (seqNum > m_relSeqNum);
  }

  else
  {
    return (m_relSeqNum < m_sequenceNumber) || (m_sequenceNumber <= m_largestSeqNum);
  }
}

int Client::sendPacket(TCPPacket *p)
{
  int packetLength;
  char *packetCString;
  int bytesSent;

  packetCString = p->getCString(packetLength);
  if (p == nullptr)
  {
    return -1;
  }

  if ((bytesSent = sendto(m_sockFd, packetCString, packetLength, 0, m_serverInfo.ai_addr, m_serverInfoLen) == -1))
  {
    std::string errorMessage = "Packet send Error: " + std::string(strerror(errno));
    // outputToStderr(errorMessage);
  }
  return bytesSent;
}

int main()
{
  std::cerr << "client is not implemented yet" << std::endl;
}
