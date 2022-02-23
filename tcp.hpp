#ifndef TCP_HPP
#define TCP_HPP
#include <string>


class TCPPacket
{
public:
  // Constructors
  TCPPacket(std::string s);
  TCPPacket(int seq, int ack, int connId, bool ackflag, bool synflag, bool finflag, int payloadLen, std::string payload);
  // Destructor
  ~TCPPacket();

  // Getter Functions
  char * getString(); 
  int getAckNum();
  int getSeqNum();
  int getConnId();
  int getPayloadLength();
  int getTotalLength();
  bool isACK();
  bool isFIN();
  bool isSYN();
  std::string getPayload();

private:
  // utility Functions
  void setString();

  // Data Members
  int m_seq, m_ack;
  int m_connId;
  int m_payloadLen;
  int m_totalLength;
  bool m_ackflag, m_synflag, m_finflag;
  std::string m_payload;
  char* m_packetCString;
};

#endif //TCP_HPP