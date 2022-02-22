#include <vector>
#include <string>
#include <unordered_map>
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
  	int writeToFile(std::string message);
  	void createSocket();
  	void sendPacket(TCPPacket* p);
  	
  	// #2
  	int handshake();
  	void addNewConnection(); // handshake will call
  	void startTimer();
  	void resetTimer();
  	void checkTimer();
  	void handleFin(TCPPacket* p);
  
  	// #3
  	void handleConnection();
  	void addPacketToBuffer(TCPPacket* p);
  	void flushBuffer();
  	
  
  	// code seq for 1 packet run
  	/*
  		 -- server setup (things like bind)
       -- In an always loop 
       		-- handle connections	
  	*/
  private:
  	int sockFd;
  	int fileFd;
    std::vector<TCPPacket*> m_packetBuffer;
  	int m_nextExpectedSeqNum;
    std::unordered_map<int, int> m_connectionTable;
  	// timer m_timer; //TODO: timer will be replaced by a different type
};