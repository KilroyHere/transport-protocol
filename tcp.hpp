#include <string>


class TCPPacket
{
public:
  TCPPacket(std::string s);
  TCPPacket(int seq, int ack, int connId, bool ackflag, bool synflag, bool finflag, int payloadLen, std::string payload);

  // get functions
  std::string getString();
  int getAckNum();
  int getSeqNum();
  int getConnId();
  int getPayloadLength();
  bool isACK();
  bool isFIN();
  bool isSYN();
  std::string getPayload();

private:
  // same variables
  int m_seq, m_ack, m_connId, m_payloadLen;
  bool m_ackflag, m_synflag, m_finflag;
  std::string m_payload;
};