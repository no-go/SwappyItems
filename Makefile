CC = g++
CPPFLAGS = -Wall -mtune=native -std=c++17
LDFLAGS = -lz -losmpbf -lprotobuf
XLFLAGS = -lstxxl -lpthread -fopenmp 

all:
	g++ SwappyItems.cpp        -O2 $(CPPFLAGS) $(LDFLAGS) -lstdc++fs -o SwappyItems.exe
	g++ STxxL_unorderedMap.cpp -O2 $(CPPFLAGS) $(LDFLAGS) $(XLFLAGS) -o STxxL_unorderedMap.exe

debug:
	g++ SwappyItems.cpp        -g -O0 -DDEBUG $(CPPFLAGS) $(LDFLAGS) -lstdc++fs -o SwappyItems.exe
	g++ STxxL_unorderedMap.cpp -g -O0 -DDEBUG $(CPPFLAGS) $(LDFLAGS) $(XLFLAGS) -o STxxL_unorderedMap.exe

clean:
	rm -rf *.exe *.o
