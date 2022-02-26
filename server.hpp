#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <netinet/in.h>
#include <bitset>
#include "constants.cpp"
#include <time.h>
class TCPPacket;

struct TCB
{
	TCB()
	{
		connectionBuffer = std::vector<char>(RWND_BYTES);
		connectionServerSeqNum = INIT_SERVER_SEQ_NUM;
		connectionExpectedSeqNum = INIT_CLIENT_SEQ_NUM;
		connectionState = -1;
		//TODO: ADD TIMER
	}

	std::vector<char> connectionBuffer;			   // Connection's payload buffer for each packet received
	std::bitset<RWND_BYTES> connectionBitvector; // A bit vector to keep track of which packets have arrived to later flush to output file
	int connectionExpectedSeqNum;				   // Next expected Seq Number from client
	int connectionServerSeqNum;					   // Seq number to be sent in server ack packet
	int connectionFileDescriptor;				   // Output target file
	time_t connectionTimer;						   // connection timer at server side (Connection closes if this runs out)
	int connectionState;
};

class Server
{
public:
	// #1
	Server(int port, std::string saveFolder);
	~Server();	// closes the socket
	void run(); // engine function of the server //#3
	int outputToStdout(std::string message);
	int outputToStderr(std::string message);
	int writeToFile(char *message, int len);
	void sendPacket(sockaddr *clientInfo, int clientInfoLen, TCPPacket *p);

	// #2
	void addNewConnection(TCPPacket *p);
	void startTimer(int connId);
	void resetTimer(int connId);
	bool checkTimer(int connId); //false if timer runs out, true if still valid
	void handleFin(TCPPacket *p);
	void closeConnection(int connId); // also will remove the connection ID entry from hashmap

	// #3
	void handleConnection();
	bool addPacketToBuffer(int connId, TCPPacket *p);
	int flushBuffer(int connId);

private:
	int m_sockFd;
	int nextAvailableConnectionId = 1;
	void closeTimedOutConnections();
	void moveWindow(int connId, int bytes);

	std::unordered_map<int, TCB *> m_connectionIdToTCB;
};

#endif //SERVER_HPP