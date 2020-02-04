all:
	g++ SwappyItems.cpp -Wall -pg -O2 -mtune=native -std=c++11 -lz -losmpbf -lprotobuf -o SwappyItems.exe

debug:
	g++ SwappyItems.cpp -Wall -g -O0 -DDEBUG -std=c++11 -lz -losmpbf -lprotobuf -o SwappyItems.exe

clean:
	rm -rf *.exe *.o
