CXX = g++
CXXFLAGS = -std=c++17 -Wall -g

all: storage client

storage:
	$(CXX) $(CXXFLAGS) src/storage.cpp -o bin/storage

client:
	$(CXX) $(CXXFLAGS) src/client.cpp -o bin/client

clean:
	rm -rf bin/*