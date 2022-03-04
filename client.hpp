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
  int readFromFile(int bytes); // returns number of bytes read
  void run(); // lol
  void handleConnection();
  void retransmitExpiredPackets(); // and reset the timer
  void setTimers(); // i don't remember what this was for
  void setTimer(int index);
  void checkTimer(int index, int seconds); // have those many seconds elapsed?
  int checkTimerAndCloseConnection();
  int checkTimersAndRetransmit();
  void dropPackets(); // also works with lseek
  void addToBuffers(std::vector<TCPPacket*> packets); // add the new packets to the buffers
  void sendPackets(); // send the packets ONLY THAT HAVE NOT BEEN SENT BEFORE
  TCPPacket* recvPacket();
  std::vector<TCPPacket*> readAndCreateTCPPackets();// -> return vector<TCPPacket*> of the new packets created
  // potential sub function: createTCPPackets(vector<char> &, int startIndex, int endIndex) that creates TCP Packets from the byte buffer
  //unlike in the server, since there is only one connection at a given time, we can ensure that each function has complete autonomy over the that connection state
  void handshake();  // hi!
  void handwave();   // bye!
  void closeConnection(); // should handle both cases where server or client needs to do FIN
  // close connection should not be called by client until all packets are not ack'ed
  int congestionControl(); // change by 1 ACK, return the amount the CWND shifted
  int shiftWindow(); // returns the number of bytes that the window has shifted
  void markAck(TCPPacket* p);
  
  /*
   * 
    resizeWindow
  */

  
private:
  int m_fileFd;
  int m_sockFd;
  int m_cwnd;
  int m_ssthresh;
  struct addrinfo m_serverInfo; // needed since we use sendto() and might have to provide server info each time
  ConnectionState m_state;
  bool m_fileRead; // file has been completely read and the winodw can't move any forward; can be a local variable in handleConnection() also
  std::vector<char> m_buffer;
  std::vector<TCPPacket*> m_packetBuffer;
  std::vector<bool> m_packetACK;
  std::vector<c_time> m_packetTimers;
  std::vector<bool> m_sentOnce;
  int m_blseek;
  int m_flseek;
  int m_largestSeqNum;
  int m_relSeqNum;
};

#endif