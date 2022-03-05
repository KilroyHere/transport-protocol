#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <chrono>
#include "tcp.hpp"
#include <netinet/in.h>
#include "constants.hpp"

typedef std::chrono::time_point<std::chrono::system_clock> c_time;

class Client
{
public:
  Client(std::string hostname, std::string port, std::string fileName);
  ~Client();
  void run(); // lol
  void handleConnection();
  bool checkTimerAndCloseConnection(); // Returns true if connection closed
  bool checkTimersforDrop(); //Return true if packets are to be dropped
  void dropPackets(); // also works with lseek
  void addToBuffers(std::vector<TCPPacket*> packets); // add the new packets to the buffers
  void sendPackets(); // send the packets ONLY THAT HAVE NOT BEEN SENT BEFORE
  TCPPacket* recvPacket();
  std::vector<TCPPacket*> readAndCreateTCPPackets();// -> return vector<TCPPacket*> of the new packets created
  // potential sub function: createTCPPackets(vector<char> &, int startIndex, int endIndex) that creates TCP Packets from the byte buffer
  //unlike in the server, since there is only one connection at a given time, we can ensure that each function has complete autonomy over the that connection state
  TCPPacket* createTCPPacket(char* buffer,int length);  
  void handshake();  // hi!
  void handwave();   // bye!
  void setTimer(TimerType type, int index = -1);
  bool checkTimer(TimerType type, float timerLimit, int index = -1);
  // potential sub function: createTCPPackets(vector<char> &, int startIndex, int endIndex) that creates TCP Packets from the byte buffer
  //unlike in the server, since there is only one connection at a given time, we can ensure that each function has complete autonomy over the that connection state
  void closeConnection(); // should handle both cases where server or client needs to do FIN
  // close connection should not be called by client until all packets are not ack'ed
  int congestionControl(); // change by 1 ACK, return the amount the CWND shifted
  int shiftWindow();       // returns the number of bytes that the window has shifted
  int markAck(TCPPacket *p);
  int sendPacket(TCPPacket *p);
  bool isDup(TCPPacket *p);
  bool allPacketsAcked();

private:
  int m_fileFd;
  int m_sockFd;
  int m_connectionId;
  int m_sequenceNumber;
  int m_largestSeqNum;
  int m_relSeqNum;
  int m_ackNumber;
  int m_cwnd;
  int m_ssthresh;
  int m_avlblwnd;

  c_time m_connectionTimer;
  c_time m_synPacketTimer;
  c_time m_finPacketTimer;
  c_time m_finEndTimer;
  struct addrinfo m_serverInfo; // needed since we use sendto() and might have to provide server info each time
  bool m_fileRead; // file has been completely read and the winodw can't move any forward; can be a local variable in handleConnection() also

  bool m_firstPacketAcked; //Set in Constructor as false, update when first packet acked
  std::vector<TCPPacket*> m_packetBuffer;
  std::vector<bool> m_packetACK;
  std::vector<c_time> m_packetTimers;
  std::vector<bool> m_sentOnce;
  int m_blseek;
  int m_flseek;

  // private function
  void printPacket(TCPPacket *p, bool recvd, bool dropped, bool dup);
  bool verifySynAck(TCPPacket *synAckPacket); // verifies the syn-ack packet of server
  bool verifyFinAck(TCPPacket *finAckPacket); // verifies fin-ack packet of server
};

#endif