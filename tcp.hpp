#ifndef TCP_HPP
#define TCP_HPP
#include <string>


class TCPPacket
{
public:
  TCPPacket(std::string s);
  TCPPacket(int seq, int ack, int connId, bool ackflag, bool synflag, bool finflag, int payloadLen, std::string payload);

  // get functions
  char * getString(); //Gets the whole Packet as a String
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
  //utility functions
  void setString();

  // same variables
  int m_seq, m_ack;
  int m_connId; // TODO: (0 - 65535) Enforce
  int m_payloadLen;
  int m_totalLength;
  bool m_ackflag, m_synflag, m_finflag;
  std::string m_payload;
  char* m_packetCString;
};

#endif //TCP_HPP