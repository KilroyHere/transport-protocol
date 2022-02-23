#include <string>
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <endian.h>
#include "tcp.hpp"
#include "constants.cpp"

/*------------------------------------------------------------
UTILITY FUNCTIONS
-------------------------------------------------------------*/

void uint32ToChar(uint32_t num, char *arr, bool bigEndian)
{
  uint32_t number;
  if (bigEndian)
    number = htobe32(num); // Changing to Big Endian
  else
    number = num; // Keeping the Endianness same
  // Setting Char Array
  arr[0] = number & 0xFF;
  arr[1] = (number >> 8) & 0xFF;
  arr[2] = (number >> 16) & 0xFF;
  arr[3] = (number >> 24) & 0xFF;
}

void uint16ToChar(uint16_t num, char *arr, bool bigEndian)
{
  uint16_t number;
  if (bigEndian)
    number = htobe16(num); // Changing to Big Endian
  else
    number = num; // Keeping the Endianness same
  // Setting Char Array
  arr[0] = number & 0xFF;
  arr[1] = (number >> 8) & 0xFF;
}

/*------------------------------------------------------------
CONSTRUCTOR
-------------------------------------------------------------*/

TCPPacket::TCPPacket(std::string s)
{
  m_totalLength = s.size();
  m_payloadLen = m_totalLength - HEADER_LEN;
  m_payload.resize(m_payloadLen);
  m_packetCString = new char[m_totalLength];
  for (int i = 0; i < m_totalLength; i++)
  {
    m_packetCString[i] = s[i]; // Can't use c_str because null bye characters
  }
  for(int i = 0 ; i < m_payloadLen; i++)
  {
    m_payload[i] = m_packetCString[i+HEADER_LEN-1];
  } 

  memcpy(&m_seq, m_packetCString, sizeof(m_seq));
  m_seq = be32toh(m_seq);

  memcpy(&m_ack, m_packetCString + 4, sizeof(m_ack));
  m_ack = be32toh(m_ack);

  memcpy(&m_connId, m_packetCString + 8, sizeof(m_connId));
  m_connId = be16toh(m_connId);

  uint16_t flagField;
  memcpy(&flagField, m_packetCString + 10, sizeof(flagField));
  flagField = be16toh(flagField);
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

  uint16_t ackbit = (m_ackflag) ? 4 : 0;
  uint16_t synbit = (m_synflag) ? 2 : 0;
  uint16_t finbit = (m_finflag) ? 1 : 0;
  uint16_t flagField = ackbit + synbit + finbit;

  // Setting Header
  uint32ToChar(seq, m_packetCString, true);
  uint32ToChar(ack, m_packetCString + 4, true);
  uint16ToChar(connId, m_packetCString + 8, true);
  uint16ToChar(flagField, m_packetCString + 10, true);

  // Setting Payload
  strcpy(m_packetCString + HEADER_LEN, m_payload.c_str());
  // Debugging Purposes
  // "|%02hhx"
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

char *TCPPacket::getString()
{
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

