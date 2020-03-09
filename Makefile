CC = g++
CPPFLAGS = -Wall -mtune=native -std=c++17
LDFLAGS = -lpthread -ltbb -lstdc++fs
LDOSMFLAGS = -lz -losmpbf -lprotobuf

all: osmread searchonhib

osmread:
	g++ example_ReadPbfData.cpp -O2 $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o ReadPbfData.exe

searchonhib:
	g++ example_SearchOnHibernateData.cpp -O2 $(CPPFLAGS) $(LDFLAGS) -o SearchOnHibernateData.exe

debug:
	g++ example_ReadPbfData.cpp -g -O0 -DDEBUG $(CPPFLAGS) $(LDFLAGS) $(LDOSMFLAGS) -o ReadPbfData.exe

clean:
	rm -rf *.exe *.o
