#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <netinet/in.h>
#include <bitset>
#include "constants.h"
class TCPPacket;



struct TCB 
{
	TCB(int expectedSeqNum, int fileDescriptor, connectionState state, bool syn, sockaddr *cInfo, socklen_t cInfoLen)
	{
		connectionBuffer = std::vector<char>(RWND_BYTES);
		connectionServerSeqNum = INIT_SERVER_SEQ_NUM;
		connectionExpectedSeqNum = expectedSeqNum;
		connectionState = state;
        clientInfo = cInfo;
        clientInfoLen = cInfoLen;
	}

	std::vector<char> connectionBuffer;			   // Connection's payload buffer for each packet received
	std::bitset<RWND_BYTES> connectionBitvector;   // A bit vector to keep track of which packets have arrived to later flush to output file
	int connectionExpectedSeqNum;				   // Next expected Seq Number from client
	int connectionServerSeqNum;					   // Seq number to be sent in server ack packet
	int connectionFileDescriptor;				   // Output target file
	c_time connectionTimer;						   // connection timer at server side (Connection closes if this runs out)
	connectionState connectionState;			   // connection state 
	TCPPacket *finPacket;
    sockaddr *clientInfo;
    socklen_t clientInfoLen;
};

class Server
{
public:
	Server(int port, std::string saveFolder);
	~Server();	// closes the socket
	void run(); // engine function of the server //#3
	int outputToStdout(std::string message);
	int outputToStderr(std::string message);
	int writeToFile(char *message, int len);
	void sendPacket(sockaddr *clientInfo, int clientInfoLen, TCPPacket *p);
	void addNewConnection(TCPPacket *p, sockaddr *clientInfo, socklen_t clientInfoLen);
	void setTimer(int connId);
	bool checkTimer(int connId, float timerLimit); //false if timer runs out, true if still valid
	void handleFin(TCPPacket *p);
	void closeConnection(int connId); // also will remove the connection ID entry from hashmap
	void handleConnection();
	bool addPacketToBuffer(int connId, TCPPacket *p);
	int flushBuffer(int connId);

private:
	int m_sockFd;
	int nextAvailableConnectionId = 1;
	void closeTimedOutConnectionsAndRetransmitFIN();
	void moveWindow(int connId, int bytes);
	std::string m_folderName;
	std::unordered_map<int, TCB*> m_connectionIdToTCB;
};

#endif //SERVER_HPP
