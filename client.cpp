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
  (Slow start) If CWND < SS-THRESH: CWND += 512
(Congestion Avoidance) If CWND >= SS-THRESH: CWND += (512 * 512) / CWND
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

int main()
{
  std::cerr << "client is not implemented yet" << std::endl;
}
