all:
	g++ SwappyItems.cpp -Wall -O2 -mtune=native -std=c++17 -lz -losmpbf -lprotobuf -o SwappyItems.exe

debug:
	g++ SwappyItems.cpp -Wall -O0 -DDEBUG -std=c++17 -lz -losmpbf -lprotobuf -o SwappyItems.exe

clean:
	rm -rf *.exe *.o
