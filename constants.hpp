#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

// client constants
const int MAX_CWND_BYTES = 51200;

// server constants
const int RWND_BYTES = 51200;
const int INIT_SERVER_SEQ_NUM = 4321;

// client constants
const int INIT_CLIENT_SEQ_NUM = 12345;

enum ConnectionState // Connection States enum
{
	AWAITING_ACK,
	CONNECTION_SET,
	FIN_RECEIVED
};

enum AddToBufferReturn
{
	PACKET_ADDED,
	PACKET_DUPLICATE,
	PACKET_DROPPED,
	PACKET_NULL
};

enum TimerType
{
	SYN_PACKET_TIMER,
	CONNECTION_TIMER,
	NORMAL_TIMER,
	FIN_PACKET_TIMER,
	FIN_END_TIMER
};

// common constants
const int MAX_PACKET_LENGTH = 524;
const int MAX_PAYLOAD_LENGTH = 512;
const int MAX_SEQ_NUM = 102400;
const int MAX_ACK_NUM = 102400;
const int HEADER_LEN = 12;
const float CONNECTION_TIMEOUT = 10; //seconds
const float RETRANSMISSION_TIMEOUT = 0.5;
const float CLIENT_CONNECTION_END_TIMEOUT = 2;

// typedefs
#endif