#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <bitset>
#include <chrono>
#include "constants.hpp"
#include "tcp.hpp"

typedef std::chrono::time_point<std::chrono::system_clock> c_time;

struct TCB
{
	TCB(int expectedSeqNum, int fileDescriptor, ConnectionState state, bool syn, struct sockaddr *cInfo, socklen_t cInfoLen)
	{
		connectionBuffer = std::vector<char>(RWND_BYTES);
		connectionServerSeqNum = INIT_SERVER_SEQ_NUM;
		connectionExpectedSeqNum = expectedSeqNum;
		previousExpectedSeqNum = -1;
		connectionState = state;
		clientInfo = cInfo;
		clientInfoLen = cInfoLen;
		finPacket = nullptr;
	}

	~TCB()
	{
		delete finPacket;
		finPacket = nullptr;
	}

	std::vector<char> connectionBuffer;					 // Connection's payload buffer for each packet received
	std::bitset<RWND_BYTES> connectionBitvector; // A bit vector to keep track of which packets have arrived to later flush to output file
	int connectionExpectedSeqNum;								 // Next expected Seq Number from client
	int previousExpectedSeqNum;
	int connectionServerSeqNum;									 // Seq number to be sent in server ack packet
	int connectionFileDescriptor;								 // Output target file
	c_time connectionTimer;											 // connection timer at server side (Connection closes if this runs out)
	ConnectionState connectionState;						 // connection state
	TCPPacket *finPacket;
	struct sockaddr *clientInfo;
	socklen_t clientInfoLen;
};

class Server
{
public:
	// #1
	Server(char *port, std::string saveFolder);
	~Server();	// closes the socket
	void run(); // engine function of the server //#3
	void outputToStdout(std::string message);
	void outputToStderr(std::string message);
	void printPacket(TCPPacket *p, bool recvd, bool dropped, bool dup);
	int writeToFile(int connId, char *message, int len);
	int sendPacket(sockaddr *clientInfo, int clientInfoLen, TCPPacket *p);

	// #2
	void addNewConnection(TCPPacket *p, sockaddr *clientInfo, socklen_t clientInfoLen);
	void setTimer(int connId);
	bool checkTimer(int connId, float timerLimit); // false if timer runs out, true if still valid
	bool handleFin(TCPPacket *p);
	void closeConnection(int connId); // also will remove the connection ID entry from hashmap
	void handleConnection();
	int addPacketToBuffer(int connId, TCPPacket *p);
	int flushBuffer(int connId);

private:
	int m_sockFd;
	int m_nextAvailableConnectionId = 1;
	void closeTimedOutConnectionsAndRetransmitFIN();
	void moveWindow(int connId, int bytes);
	std::string m_folderName;
	std::unordered_map<int, TCB *> m_connectionIdToTCB;
};

#endif // SERVER_HPP
