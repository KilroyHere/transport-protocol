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


Client::Client(std::string hostname, std::string port, std::string fileName)
{
  using namespace std;
  struct addrinfo hints, *servInfo, *p;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // set to AF_INET to use IPv4
  hints.ai_socktype = SOCK_DGRAM;

  m_blseek = 0;
  m_flseek = 0;
  m_largestSeqNum = 0;
  m_relSeqNum = 0;
  m_cwnd = INIT_CLIENT_SEQ_NUM;
  m_ssthresh = INITIAL_SSTHRESH;

  int ret;
  if ((ret = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &servInfo)) != 0)
  {
    cerr << "ERROR: in getaddrinfo " << strerror(errno) << endl;
    exit(1);
  }
  int sockfd = -1;

  for (p = servInfo; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      cerr << "ERROR: in socket " << strerror(errno) << endl;
      continue;
    }
    break;
  }
  // non blocking receiving
  if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
  {
    cerr << "ERROR: in fcntl " << strerror(errno) << endl;
    exit(1);
  }

  if (p == NULL)
  {
    cerr << "ERROR: in socket " << strerror(errno) << endl;
    exit(1);
  }

  m_serverInfo = *p;
  freeaddrinfo(servInfo); // free the next, keep the current;
  servInfo = NULL;
}

Client::~Client()
{
  // assumption is that closeConnections() was called before already, so the destructor has to do nothing
}

/**
 * @brief Reading Available Window and Create TCP Packets
 * 
 * @return std::vector<TCPPacket *> 
 */
std::vector<TCPPacket *> Client::readAndCreateTCPPackets()
{
  std::vector<TCPPacket *> packets; //Creating Packets to send
  char *fileBuffer = new char[m_avlblwnd]; //File buffer of size Available Window
  int bytesRead; // Bytes Read
  if(lseek(m_fileFd, m_flseek, SEEK_SET) == -1) // SEEK_SET start from start of file 
  {
    std::string errorMessage = "ERROR: File lseek error : " + std::string(strerror(errno));
    std::cerr << errorMessage << std::endl;
    exit(1);
  }

  if ((bytesRead = read(m_fileFd, fileBuffer, m_avlblwnd)) == -1)
  {
    std::string errorMessage = "ERROR: File write Error: " + std::string(strerror(errno));
    std::cerr << errorMessage << std::endl;
    exit(1);
  }
  m_flseek += bytesRead; //Next time start reading from this position
  int indexIntoFileBuffer = 0; //Index to specify packet starting point
  while (indexIntoFileBuffer < bytesRead)
  {
    int length = ((bytesRead - indexIntoFileBuffer) > MAX_PAYLOAD_LENGTH) ? MAX_PAYLOAD_LENGTH : bytesRead - indexIntoFileBuffer;

    TCPPacket *p = createTCPPacket(fileBuffer + indexIntoFileBuffer, length);
    m_sequenceNumber = (m_sequenceNumber + length) % (MAX_SEQ_NUM+1); // Updating sequence number using packet length
    packets.push_back(p);
    indexIntoFileBuffer += length;
  }
  if(!packets.empty())
    m_largestSeqNum = packets[packets.size() - 1]->getSeqNum(); //Largest Sequence Number is the sequence_no of last packet created
  delete fileBuffer;
  return packets;
}

/**
 * @brief Creates TCP Packet from payload
 * 
 * @param buffer 
 * @param length 
 * @return TCPPacket* 
 */
TCPPacket *Client::createTCPPacket(char *buffer, int length)
{
  // Ack handler
  bool ackFlag;
  if (!m_firstPacketAcked)
    ackFlag = true;
  else
    ackFlag = false;
  int ackNo = ackFlag ? m_ackNumber : 0;

  std::string payload = convertCStringtoStandardString(buffer, length);
  TCPPacket *p = new TCPPacket(
      m_sequenceNumber,
      ackNo,
      m_connectionId,
      ackFlag,
      false,
      false,
      payload.size(),
      payload);
  return p;
}

/**
 * @brief Checks retransmission timer for all packets
 * 
 * @return true 
 * @return false 
 */
bool Client::checkTimersforDrop()
{
  for(long unsigned int i = 0 ; i < m_packetTimers.size() ; i++)
  {
    if(!checkTimer(NORMAL_TIMER,RETRANSMISSION_TIMEOUT,i) && m_packetACK[i] == false)
    {
      m_ssthresh = m_cwnd / 2;
      m_cwnd = MAX_PAYLOAD_LENGTH;
      return true; //Need to drop packet here
    }
  }
  return false;
}

/**
 * @brief Drops all packets
 * 
 */
void Client::dropPackets()
{
  TCPPacket *packet;
  while(!m_packetBuffer.empty()) //Deletes all TCPPackets from the buffer
  {
    packet = m_packetBuffer.back();
    m_packetBuffer.pop_back();
    delete packet;
  }
  m_packetTimers.clear(); 
  m_packetACK.clear();
  m_sequenceNumber = m_blseek % (MAX_SEQ_NUM + 1); // Sequence number goes to m_blseek
  m_flseek = m_blseek; // Forward lseek goes back to m_blseek
}

/**
 * @brief Checks Connection Timer and clses connection in case of lapse
 * 
 * @return true 
 * @return false 
 */
bool Client::checkTimerAndCloseConnection()
{
  if (!checkTimer(CONNECTION_TIMER, CONNECTION_TIMEOUT))
  {
    closeConnection();
    return true;
  }
  return false;
}

/**
 * @brief Checks Timer passed in parameters
 * 
 * @param type 
 * @param timerLimit 
 * @param index 
 * @return true 
 * @return false 
 */
bool Client::checkTimer(TimerType type, float timerLimit, int index)
{
  c_time current_time = std::chrono::system_clock::now();
  c_time start_time;
  switch (type)
  {
    case CONNECTION_TIMER:
    {
      start_time = m_connectionTimer;
      break;
    }
    case NORMAL_TIMER:
    {
      if(index == -1)
      {
        std::cerr << "ERROR: Incorrect Index for Timer" << std::endl;
        exit(1);
      }
      start_time = m_packetTimers[index];
      break;
    }
    case SYN_PACKET_TIMER:
    {
      start_time = m_synPacketTimer;
      break;
    }
    case FIN_PACKET_TIMER:
    {
      start_time = m_finPacketTimer;
      break;
    }
    case FIN_END_TIMER:
    {
      start_time = m_finEndTimer;
      break;
    }
    default:
    {
      std::cerr << "Incorrect Timer Type" << std::endl;
      exit(1);
    }
  }
 
  std::chrono::duration<double> elapsed_time = current_time - start_time;
  int elapsed_seconds = elapsed_time.count();
  return (elapsed_seconds < timerLimit); // Returns false when timer has elapsed
}

/**
 * @brief Close Connection
 * 
 */
void Client::closeConnection()
{
  close(m_sockFd);
  close(m_fileFd);
  return;
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

  for (int i = 0; i < (int) m_packetBuffer.size(); i++)
  {
    if (!m_packetACK[i])
      break;
    shiftedIndices++;
    shiftedBytes += m_packetBuffer[i]->getPayloadLength();
  }

  // shift the values ahead
  for (int i = shiftedIndices; i < (int) m_packetBuffer.size(); i++)
  {
    delete m_packetBuffer[i - shiftedIndices];
    m_packetBuffer[i - shiftedIndices] = nullptr;
    m_packetBuffer[i - shiftedIndices] = m_packetBuffer[i];
    m_packetACK[i - shiftedIndices] = m_packetACK[i];
    m_packetTimers[i - shiftedIndices] = m_packetTimers[i];
    m_sentOnce[i - shiftedIndices] = m_sentOnce[i];
  }

  // initialize the values at the end
  for (int i = m_packetBuffer.size() - shiftedIndices; i < (int) m_packetBuffer.size(); i++)
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

int Client::markAck(TCPPacket *p)
{
  using namespace std;
  if (p == nullptr)
  {
    cerr << "Unexpected nullptr found in Client::markAck" << endl;
    return PACKET_NULL;
  }
  int ack = p->getAckNum();

  // currently no implementation of what to do when ACK is beyond the window
  // since it is not clear which function to bring that into

  if (m_packetBuffer.empty())
    return PACKET_DROPPED;

  int windowBegin = m_packetBuffer[0]->getSeqNum();
  int windowEnd = m_packetBuffer[0]->getSeqNum();
  bool wrapAroundAck = false;
  if (
      (windowBegin < windowEnd && windowBegin <= ack && ack < windowEnd) ||
      (windowBegin > windowEnd && (windowBegin <= ack || ack < windowEnd))
    )
    {
      if(ack < windowBegin && ack <= windowEnd)
      {
        wrapAroundAck = true;
      }
      // potential code that says that ack is valid
    }
  else
  {
    return PACKET_DROPPED;
  }
  
  for (int i = 0; i < (int) m_packetBuffer.size(); i++)
  {
    if (wrapAroundAck && m_packetBuffer[i]->getSeqNum() > windowEnd) 
      m_packetACK[i] = true;
    else if (ack < m_packetBuffer[i]->getSeqNum())
      break;
    else
      m_packetACK[i] = true;
  }

  return PACKET_ADDED; // ACK changes were successfully added to the buffer

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

/**
 * @brief Prints the given packet
 * 
 * @param p 
 * @param recvd 
 * @param dropped 
 * @param dup 
 */
void Client::printPacket(TCPPacket *p, bool recvd, bool dropped, bool dup)
{
  std::string message;

  if(recvd)
    message = "RECV";
  else
    message = "SEND";

  if (recvd && dropped)
    message = "DROP";

  message = message + " " + std::to_string(p->getSeqNum()) + " " + std::to_string(p->getAckNum()) + " " + std::to_string(p->getConnId()) + " ";
  if(p->isACK())
    message += "ACK ";
  if(p->isSYN())
    message += "SYN ";
  if(p->isFIN())
    message += "FIN ";
  if(dup && !recvd)
    message += "DUP ";
  message.pop_back(); // remove the trailing space
  std::cout << std::endl; 
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

  TCPPacket *synAckPacket = nullptr;
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
    if (!checkTimer(SYN_PACKET_TIMER, RETRANSMISSION_TIMEOUT))
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
    if (!checkTimer(FIN_PACKET_TIMER, RETRANSMISSION_TIMEOUT))
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
  while (checkTimer(FIN_END_TIMER, CLIENT_CONNECTION_END_TIMEOUT))
  {
    serverFinPacket = recvPacket(); // recv the fin packet

    // if packet received and is in good form then break from loop
    if (serverFinPacket != nullptr && serverFinPacket->isFIN())
    {
      sendPacket(clientAckPacket);
      delete serverFinPacket;
      serverFinPacket = nullptr;
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
  for (int i = 0; i < (int) m_sentOnce.size(); i++)
  {
    // We send the packets which are marked as false in sentOnce
    if (!m_sentOnce[i])
    {
      sendPacket(m_packetBuffer[i]);                        // send packets
      m_sentOnce[i] = true;                                 // set sentOnce to true
      m_packetTimers[i] = std::chrono::system_clock::now(); // start timer

      bool isDuplicate = isDup(m_packetBuffer[i]); // check if the packet is a duplicate packet;
      printPacket(m_packetBuffer[i], false, false, isDuplicate);
    }
  }
}

/**
 * @brief Determines if a packet is a Dup or not 
 * 
 * @param p TCP Packet inspected 
 */
bool Client::isDup(TCPPacket *p)
{
  int seqNum = p->getSeqNum();
  if (m_largestSeqNum < m_relSeqNum)
  {
    return (seqNum <= m_largestSeqNum) || (seqNum > m_relSeqNum);
  }

  else
  {
    return (m_relSeqNum < seqNum) || (seqNum <= m_largestSeqNum);
  }
}

/**
 * @brief sends TCP packet 
 * 
 * @param p The TCP Packet to send 
 * @return int bytes send 
 */
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

  if ((bytesSent = sendto(m_sockFd, packetCString, packetLength, 0, m_serverInfo.ai_addr, m_serverInfo.ai_addrlen) == -1))
  {
    std::string errorMessage = "Packet send Error: " + std::string(strerror(errno));
    // outputToStderr(errorMessage);
  }
  return bytesSent;
}

// SYN_PACKET_TIMER,
// CONNECTION_TIMER,
// NORMAL_TIMER,
// FIN_PACKET_TIMER,
// FIN_END_TIMER

/**
 * @brief set timer by saving current timestamp
 */
void Client::setTimer(TimerType type, int index)
{
  switch (type)
  {
  case SYN_PACKET_TIMER:
    /* code */
    m_synPacketTimer = std::chrono::system_clock::now();
    break;

  case CONNECTION_TIMER:
    /* code */
    m_connectionTimer = std::chrono::system_clock::now();
    break;

  case FIN_PACKET_TIMER:
    /* code */
    m_finPacketTimer = std::chrono::system_clock::now();
    break;

  case FIN_END_TIMER:
    /* code */
    m_finEndTimer = std::chrono::system_clock::now();
    break;

  default:
    break;
  }
}

bool Client::allPacketsAcked() 
{
  for (auto i : m_packetACK)
  {
    if (!i) return false;
  }
  return true;
}

/**
 * @brief Entry point for running client services
 * 
 */
void Client::run()
{
  using namespace std;
  handshake();
  while(true)
  {
    // connection closing here were close due to timeout and will 
    // not involve sending of any fin packets 
    if(checkTimerAndCloseConnection())
      return;
    bool drop = checkTimersforDrop();
    if (drop) 
      m_avlblwnd = MAX_PAYLOAD_LENGTH; // reset available window to 1 packet size in case of drop
    vector<TCPPacket*> newPackets = readAndCreateTCPPackets();

    if (m_avlblwnd == 0 && newPackets.size() == 0 && allPacketsAcked())
    {
      break;  // reached end of file, nothing more to read, and nothing new to receive
    }

    addToBuffers(newPackets);
    sendPackets();
    
    // recvs only 1 packet per iteration of the loop 
    TCPPacket* p = recvPacket();
    if (p == nullptr)  // unblocking receive call did not receive any packet in the current iteration
      continue; // no other states will be changed
    
    setTimer(CONNECTION_TIMER); // received a message from the server, reset the connection timer

    // call handle fin 
    int packetStatus = markAck(p);

    bool packetDropped = packetStatus == PACKET_DROPPED;
    printPacket(p, true, packetDropped, false);
    
    int shifted = shiftWindow();
    int cwndChange = congestionControl();
    m_avlblwnd = shifted + cwndChange;
  } 
  handwave();
}

int main(int argc, char * argv[])
{
  using namespace std;
  if (argc != 4)
  {
    cout << "ERROR: Incorrect number of arguments provided!" << endl;
    exit(1);
  }

  if (!(atoi(argv[2])))
  {
    cout << "ERROR: Incorrect format of ports provided" << endl;
    exit(1); //TODO: need to change and implement the exact exit functions with different exit codes.
  }

  Client client(argv[1], argv[2], argv[3]);
  client.run();
}