CC = g++
CPPFLAGS = -Wall -mtune=native -std=c++17 -I./src/
LDFLAGS = -lpthread -ltbb -lstdc++fs -lm
LDOSMFLAGS = -lz -losmpbf -lprotobuf

all: osm such hib gra

gra:
	g++ examples/osm2graph.cpp -O6 -DDEBUG $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o osm2graph.exe

osm:
	g++ examples/ReadPbfData.cpp -O2 $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o ReadPbfData.exe

such:
	g++ examples/such.cpp -O6 -DDEBUG $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o such.exe

hib:
	g++ examples/SearchOnHibernateData.cpp -O2 $(CPPFLAGS) $(LDFLAGS) -o SearchOnHibernateData.exe

docs: Doxyfile README.md examples/* src/*
	rm -rf docs
	doxygen Doxyfile

debug:
	g++ examples/ReadPbfData.cpp -g -O0 -DDEBUG $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o ReadPbfData_Debug.exe

clean:
	rm -rf *.exe *.o
