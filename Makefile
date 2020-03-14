CC = g++
CPPFLAGS = -Wall -mtune=native -std=c++17 -I./src/
LDFLAGS = -lpthread -ltbb -lstdc++fs
LDOSMFLAGS = -lz -losmpbf -lprotobuf

all: osmread searchonhib

osmread:
	g++ examples/ReadPbfData.cpp -O2 $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o ReadPbfData.exe

searchonhib:
	g++ examples/SearchOnHibernateData.cpp -O2 $(CPPFLAGS) $(LDFLAGS) -o SearchOnHibernateData.exe


doc: Doxyfile README.md
	doxygen Doxyfile

debug:
	g++ examples/ReadPbfData.cpp -g -O0 -DDEBUG $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o ReadPbfData_Debug.exe

clean:
	rm -rf *.exe *.o
