CC = g++
CPPFLAGS = -Wall -std=c++17
LDFLAGS = -lpthread -ltbb -lstdc++fs

all:
	g++ msearch.cpp -O2 $(CPPFLAGS) $(LDFLAGS) -o msearch.exe

clean:
	rm -rf *.exe *.o
