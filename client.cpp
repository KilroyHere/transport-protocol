#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <vector>
#include <chrono>
#include <iostream>
#include "constants.hpp"
#include "tcp.hpp"
#include "client.hpp"

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

  for (int i=0; i<m_packetBuffer.size(); i++)
  {
    if (! m_packetACK[i]) break;
    shiftedIndices++;
    shiftedBytes += m_packetBuffer[i]->getPayloadLength();
  }

  // shift the values ahead
  for (int i=shiftedIndices; i < m_packetBuffer.size(); i++)
  {
    delete m_packetBuffer[i - shiftedIndices];
    m_packetBuffer[i - shiftedIndices] = nullptr;
    m_packetBuffer[i - shiftedIndices] = m_packetBuffer[i];
    m_packetACK[i - shiftedIndices] = m_packetACK[i];
    m_packetTimers[i - shiftedIndices] = m_packetTimers[i];
    m_sentOnce[i - shiftedIndices] = m_sentOnce[i];
  }

  // initialize the values at the end
  for (int i= m_packetBuffer.size() - shiftedIndices; i < m_packetBuffer.size(); i++)
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

void Client::markAck(TCPPacket* p)
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

  for (int i=0; i<m_packetBuffer.size(); i++)
  {
    if (ack < m_packetBuffer[i]->getSeqNum())
      break;
    else
      m_packetACK[i] = true;
  }
}

int main()
{
  std::cerr << "client is not implemented yet" << std::endl;
}
