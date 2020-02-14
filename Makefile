CC = g++
CPPFLAGS = -Wall -mtune=native -std=c++17
LDFLAGS = -lstxxl -lpthread -fopenmp -lz -losmpbf -lprotobuf

all:
	g++ SwappyItems.cpp -O2 $(CPPFLAGS) $(LDFLAGS) -o SwappyItems.exe

debug:
	g++ SwappyItems.cpp -g -O0 -DDEBUG $(CPPFLAGS) $(LDFLAGS) -o SwappyItems.exe

clean:
	rm -rf *.exe *.o
