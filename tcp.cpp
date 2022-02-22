#include <string>
#include <cstring>
#include <sys/types.h>
#include <endian.h>
#include "tcp.hpp"
#include "constants.cpp"

void uint32ToChar(uint32_t num, unsigned char *arr, bool bigEndian)
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

void uint16ToChar(uint16_t num, unsigned char *arr, bool bigEndian)
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

TCPPacket::TCPPacket(std::string s)
{
  m_totalLength = s.size();
  m_packetCString = new char[m_totalLength];
  memset(m_packetCString, '\0', sizeof(m_packetCString)); // Setting all bytes to null bytes
  strcpy(m_packetCString,s.c_str());

}

TCPPacket::TCPPacket(int seq, int ack, int connId, bool ackflag, bool synflag, bool finflag, int payloadLen, std::string payload)
{
  m_seq = seq;
  m_ack = ack;
  m_connId = connId;
  m_ackflag = ackflag;
  m_finflag = finflag;
  m_payloadLen = payloadLen;
  m_payload = payload;
  m_totalLength = payloadLen + 12;
  m_packetCString = new char[m_totalLength];
  memset(m_packetCString, '\0', sizeof(m_packetCString)); // Setting all bytes to null bytes
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
  uint16ToChar(flagField, m_packetCString + 10, false);
  // Setting Payload
  strcpy(m_packetCString + 12, m_payload.c_str());
}

char *TCPPacket::getString()
{
  return m_packetCString;
}

TCPPacket::~TCPPacket()
{
  delete m_packetCString;
}