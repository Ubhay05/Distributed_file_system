CXX = g++
CXXFLAGS = -std=c++17 -Wall -g -pthread

all: storage client master

storage:
	$(CXX) $(CXXFLAGS) src/storage.cpp -o bin/storage

client:
	$(CXX) $(CXXFLAGS) src/client.cpp -o bin/client

master:
	$(CXX) $(CXXFLAGS) src/master.cpp -o bin/master

clean:
	rm -rf bin/*