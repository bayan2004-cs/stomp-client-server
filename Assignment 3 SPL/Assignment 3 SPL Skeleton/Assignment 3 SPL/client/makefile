# Default Boost path
# Linux (standard /usr/include):
BOOST_INCLUDE_PATH ?= 
# Windows (uncomment to use):
BOOST_INCLUDE_PATH := C:/boost_1_90_0

# Include path (works for both if BOOST_INCLUDE_PATH is set correctly)
CFLAGS:=-c -Wall -Weffc++ -g -std=c++11 -Iinclude -I$(BOOST_INCLUDE_PATH)

# Linker flags
# Linux:
# LDFLAGS:=-lpthread
# Windows (uncomment to use):
LDFLAGS:=-lws2_32 -lwsock32

all: EchoClient

EchoClient: bin/ConnectionHandler.o bin/echoClient.o
	g++ -o bin/EchoClient bin/ConnectionHandler.o bin/echoClient.o $(LDFLAGS)

StompWCIClient: bin/ConnectionHandler.o bin/StompClient.o bin/event.o bin/StompProtocol.o
	g++ -o bin/StompWCIClient bin/ConnectionHandler.o bin/StompClient.o bin/event.o bin/StompProtocol.o $(LDFLAGS)

bin/ConnectionHandler.o: src/ConnectionHandler.cpp
	g++ $(CFLAGS) -o bin/ConnectionHandler.o src/ConnectionHandler.cpp

bin/echoClient.o: src/echoClient.cpp
	g++ $(CFLAGS) -o bin/echoClient.o src/echoClient.cpp

bin/event.o: src/event.cpp
	g++ $(CFLAGS) -o bin/event.o src/event.cpp

bin/StompClient.o: src/StompClient.cpp
	g++ $(CFLAGS) -o bin/StompClient.o src/StompClient.cpp

bin/StompProtocol.o: src/StompProtocol.cpp
	g++ $(CFLAGS) -o bin/StompProtocol.o src/StompProtocol.cpp

.PHONY: clean
clean:
	rm -f bin/*