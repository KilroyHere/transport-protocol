#include <string>
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <endian.h>
#include "tcp.hpp"
#include "constants.cpp"

/*------------------------------------------------------------
CONSTRUCTORS
-------------------------------------------------------------*/

TCPPacket::TCPPacket(std::string s)
{
  m_totalLength = s.size();
  m_packetString = s;
  m_packetCString = new char[m_totalLength];
  for (int i = 0; i < m_totalLength; i++)
  {
    m_packetCString[i] = s[i]; // Can't use c_str because null bye characters
  }

  m_payloadLen = m_totalLength - HEADER_LEN;
  m_payload.resize(m_payloadLen);
  
  for(int i = 0 ; i < m_payloadLen; i++)
  {
    m_payload[i] = m_packetCString[i+HEADER_LEN];
  } 

  memcpy(&m_seq, m_packetCString, sizeof(m_seq));
  m_seq = be32toh(m_seq);

  memcpy(&m_ack, m_packetCString + 4, sizeof(m_ack));
  m_ack = be32toh(m_ack);

  memcpy(&m_connId, m_packetCString + 8, sizeof(m_connId));
  m_connId = be16toh(m_connId);

  char flagField = m_packetCString[11];

  m_ackflag = ((flagField & 4) >> 2) ? true : false; // 4 = b100
  m_synflag = (flagField & 2) >> 1 ? true : false;   // 2 = b010
  m_finflag = (flagField & 1) ? true : false;        // 1 = b001
}

TCPPacket::TCPPacket(int seq, int ack, int connId, bool ackflag, bool synflag, bool finflag, int payloadLen, std::string payload)
{
  m_seq = seq;
  m_ack = ack;
  m_connId = connId;
  m_ackflag = ackflag;
  m_synflag = synflag;
  m_finflag = finflag;
  m_payloadLen = payloadLen;
  m_payload = payload;
  m_totalLength = payloadLen + HEADER_LEN;
  m_packetCString = new char[m_totalLength];
  setString();
}

void TCPPacket::setString()
{
  uint32_t seq = (uint32_t)m_seq;
  uint32_t ack = (uint32_t)m_ack;
  uint16_t connId = (uint16_t)m_connId;

  char ackbit = (m_ackflag) ? 4 : 0;
  char synbit = (m_synflag) ? 2 : 0;
  char finbit = (m_finflag) ? 1 : 0;
  char flagField = ackbit + synbit + finbit;

  // Setting Header
  seq = htobe32(seq);
  memcpy(m_packetCString,&seq,sizeof(seq));

  ack = htobe32(ack);
  memcpy(m_packetCString+4,&ack,sizeof(ack));

  connId = htobe16(connId);
  memcpy( m_packetCString+8,&connId,sizeof(connId));

  m_packetCString[10] = 0;
  m_packetCString[11] = flagField;

  // Setting Payload
  for(int i = 0 ; i < m_payloadLen; i++)
  {
    m_packetCString[i+12] = m_payload[i];
  }

  m_packetString.resize(m_totalLength);
  for(int i = 0 ; i < m_totalLength; i++)
  {
    m_packetString[i] = m_packetCString[i];
  }
}

/*------------------------------------------------------------
DESTRUCTOR
-------------------------------------------------------------*/

TCPPacket::~TCPPacket()
{
  delete m_packetCString;
}

/*------------------------------------------------------------
GETTER FUNCTIONS
-------------------------------------------------------------*/
std::string TCPPacket::getString()
{
  return m_packetString;
}

char *TCPPacket::getCString(int &length) //Currently returns pointer to m_packetCString
{
  length = m_totalLength;
  return m_packetCString;
}


int TCPPacket::getAckNum()
{
  return m_ack;
}

int TCPPacket::getSeqNum()
{
  return m_seq;
}

int TCPPacket::getConnId()
{
  return m_connId;
}

int TCPPacket::getPayloadLength()
{
  return m_payloadLen;
}

int TCPPacket::getTotalLength()
{
  return m_totalLength; 
}

bool  TCPPacket::isACK()
{
  return m_ackflag;
}
bool TCPPacket::isFIN()
{
  return m_finflag;
}
bool TCPPacket::isSYN()
{
  return m_synflag;
}

std::string TCPPacket::getPayload()
{
  return m_payload;
}

