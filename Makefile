CC = g++
CPPFLAGS = -Wall -mtune=native -std=c++17 -I./src/
LDFLAGS = -lpthread -ltbb -lstdc++fs
LDOSMFLAGS = -lz -losmpbf -lprotobuf

all: osmread searchonhib

osmread:
	g++ examples/ReadPbfData.cpp -O2 $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o ReadPbfData.exe

such:
	g++ examples/such.cpp -O6 -DDEBUG $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o such.exe

searchonhib:
	g++ examples/SearchOnHibernateData.cpp -O2 $(CPPFLAGS) $(LDFLAGS) -o SearchOnHibernateData.exe


docs: Doxyfile README.md
	rm -rf docs
	doxygen Doxyfile

debug:
	g++ examples/ReadPbfData.cpp -g -O0 -DDEBUG $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o ReadPbfData_Debug.exe

clean:
	rm -rf *.exe *.o
