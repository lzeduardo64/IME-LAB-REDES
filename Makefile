CPP_FLAGS = -Wall -Wextra

.PHONY: all clean

all: server client

server: server.cpp
	$(CXX) server.cpp $(CPP_FLAGS) -o server

client: client.cpp
	$(CXX) client.cpp $(CPP_FLAGS) -o client


clean:
	rm server client
