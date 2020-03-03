CC = g++
CPPFLAGS = -Wall -std=c++17
LDFLAGS = -lpthread -ltbb -lstdc++fs

all:
	g++ msearch.cpp -O0 -g $(CPPFLAGS) $(LDFLAGS) -o msearch.exe

clean:
	rm -rf *.exe *.o
