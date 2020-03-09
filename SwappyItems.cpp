// we need it for PRId32 in snprintf
#define __STDC_FORMAT_MACROS

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <utility> // pair
#include <chrono>

#include <inttypes.h>
#include <vector>
#include <thread> // just for printing the number of cores
#include <atomic>

// catch ctrl+c
#include <csignal>
#include <unistd.h>

#include "SwappyItems.hpp"
#include "osmpbfreader.h"

using namespace CanalTP;
using namespace std;

#define FILE_ITEMS           (   2*1024)
#define FILE_MULTI                    4
#define RAM_MULTI                     8
#define BBITS                         5
#define BMASK   ((BBITS+4)*  FILE_ITEMS)

typedef uint64_t Key; // for the key-value tuple, 8 byte

struct Value {
    uint8_t _type;
    uint8_t _uses;
    double _lon;
    double _lat;
    Key _parent; // if we want to store the whole ref list, then hiberate and file swap must be modified for collection vars !
    char _name[256];
};

void ValueSet (Value & v, Key parent = 0, double lon = 0.0, double lat = 0.0, uint8_t typ = 0, uint8_t uses = 0) {
    v._uses = uses;
    v._lon = lon;
    v._lat = lat;
    v._type = typ;
    v._parent = parent;
}

typedef SwappyItems<Key, Value, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> KVstore;
KVstore * ways;

// ---------------------------------------------------------------------

std::chrono::time_point<std::chrono::system_clock> start;
double mseconds;
atomic<bool> isPrinted = false;

#include "tools.hpp"
#include "Routing.hpp"

/*
unsigned maxref = 0;

struct Routing {

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {
        Value dummy;

        if (catchWay(dummy, osmid, tags)) { // fills in basis way data
            if (refs.size() > maxref) maxref = refs.size();
        }
    }

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {}
    
    void relation_callback (uint64_t osmid, const Tags & tags, const References & refs){}
};
*/


int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage: %s file_to_read.osm.pbf\n", argv[0]);
        return 1;
    }
    
    ways = new KVstore(23);
    Routing routing;
    
    logHead();
    catchSig();

    start = std::chrono::high_resolution_clock::now();

    read_osm_pbf(argv[1], routing, true); // read way only
    
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("# end ways: %.f ms\n", mseconds);
    //printf("maxref: %u\n", maxref);

    read_osm_pbf(argv[1], routing, false); // read nodes and relations
    
    now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("# end nodes: %.f ms\n", mseconds);
    
    // stores final data structure!
    ways->hibernate();

    delete ways;
    return 0;
}

