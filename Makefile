CXX=g++
CXXOPTIMIZE= -O2
CXXFLAGS= -g -Wall -pthread -std=c++11 
USERID=123456789
CLASSES=

all: server client

server: $(CLASSES)
	$(CXX) -o server $(CXXFLAGS) server.cpp tcp.cpp

client: $(CLASSES)
	$(CXX) -o client $^ $(CXXFLAGS) clinet.cpp tcp.cpp

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

dist: tarball
tarball: clean
	tar -cvzf /tmp/$(USERID).tar.gz --exclude=./.vagrant . && mv /tmp/$(USERID).tar.gz .
