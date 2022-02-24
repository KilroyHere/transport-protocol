#include <vector>
#include <string>
#include <unordered_map>
#include <netinet/in.h>
#include <bitset>
#include "constants.cpp"
#include <time.h>
class TCPPacket;

class Server
{
  public:
  	// #1
  	Server(int port, std::string saveFolder);
  	~Server(); // closes the socket
  	void run(); // engine function of the server
  	int outputToStdout(std::string message);
  	int outputToStderr(std::string message);
  	int writeToFile(char* message, int len);
  	void createSocket();
  	void sendPacket(sockaddr* clientInfo, int clientInfoLen, TCPPacket* p);
  	
  	// #2
  	int handshake();
  	void addNewConnection(TCPPacket* p); // handshake will call, initalize sequence number to 4321, ensure that vector is allocated of RWND_BYTES
  	void startTimer(int connId);
  	void resetTimer(int connId);
  	bool checkTimer(int connId); //false if timer runs out, true if still valid
  	void handleFin(TCPPacket* p); 
    void closeConnection(int connId); // also will remove the connection ID entry from hashmap
  	// #3
  	void handleConnection();
  	bool addPacketToBuffer(int connId, TCPPacket* p);
  	int flushBuffer(int connId);
  	
  private:
  	int m_sockFd;

	void closeTimedOutConnections();
	void moveWindow(int connId, int bytes);

    std::unordered_map<int, std::vector<char> > m_connectionIdToBuffer; // maps connection ID to packet buffer
	std::unordered_map<int, std::bitset<RWND_BYTES> > m_connectionBitvector; // maps connection ID to packet buffer bitvector
    std::unordered_map<int,int> m_connectionExpectedSeqNums; // maps connection ID to that connection's next expected sequence
    std::unordered_map<int,int> m_connectionServerSeqNums; // maps connection ID to that connection's 
    std::unordered_map<int,int> m_connectionFileDescriptor;
  	std::unordered_map<int, time_t> m_connectionTimer; //TODO: timer will be replaced by a different type
};