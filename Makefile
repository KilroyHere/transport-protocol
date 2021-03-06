CXX=g++
CXXOPTIMIZE= -O2
CXXFLAGS= -g -Wall -pthread -std=c++11 $(CXXOPTIMIZE)
USERID=123456789
CLASSES=

all: server client

server: $(CLASSES)
	$(CXX) -o server $(CXXFLAGS) server.cpp tcp.cpp utilities.cpp

client: $(CLASSES)
	$(CXX) -o client $^ $(CXXFLAGS) client.cpp tcp.cpp utilities.cpp

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

dist: tarball
tarball: clean
	tar -cvzf /tmp/$(USERID).tar.gz --exclude=./.vagrant . && mv /tmp/$(USERID).tar.gz .
