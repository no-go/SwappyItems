// try to store many nodes info beyond the RAM !?!

// g++ SwappyItems.cpp -Wall -O2 -mtune=native -lz -losmpbf -lprotobuf -std=c++11 -o SwappyItems.exe
// g++ %f -Wall -O2 -mtune=native -std=c++11 libz.a libosmpbf.a libprotobuf.a -o %e.exe

#define __STDC_FORMAT_MACROS
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <inttypes.h>
#include <vector>
#include <chrono>

#include "SwappyItems.hpp"
#include "osmpbfreader.h"

using namespace CanalTP;
using namespace std;

//#define MAX_NODES     2405814605.0 // EUROPE
//#define MAX_NODES       67853945.0 // NRW (Node mit 18byte sollte dann 1,2 GB RAM brauchen)

typedef uint64_t Key; // for the key-value tuple

struct Value {
    uint8_t _uses; // 1byte
    double _lon;   // 8byte
    double _lat;   // 8byte
    bool _town;    // 1byte
};

void ValueSet (Value & v, double lon = 0, double lat = 0, bool town = false, uint8_t uses = 0) {
    v._uses = uses;
    v._lon = lon;
    v._lat = lat;
    v._town = town;
}

SwappyItems<Key,Value,(16*1024),4,4> nodes;

// ---------------------------------------------------------------------

typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds Millis;
Clock::time_point tipo0;

int parseLine(char* line) {
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}

int getUsedKB() {
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result;
}

void addNode(Key & osmid, const double & lon, const double & lat, bool town) {
    Value * node = nodes.get(osmid);

    auto tipo1 = Clock::now();
    if (std::chrono::duration_cast<Millis>(tipo1 - tipo0).count() >= 2000) {
        printf(
            "%10" PRId64 " %10" PRId64 " "
            "%10" PRId64 " "
            "%13" PRId64 " "
            "%10" PRId64 " "
            "%13" PRId64 " "
            "%10" PRId64 " "
            "%10" PRId64 " "
            "%10d\n",
            nodes.size(), nodes.size(true),
            nodes.updates,
            nodes.bloomSaysFresh, nodes.bloomFails,
            nodes.rangeSaysNo, nodes.rangeFails, nodes.fileLoads,
            getUsedKB()
        );
        tipo0 = tipo1;
    }

    if (node == nullptr) {
        Value dummy;
        ValueSet(dummy, lon, lat, town);
        nodes[osmid] = dummy;
    } else {
        //printf("%" PRId64 " reused\n", osmid);
        node->_uses++;
        nodes.set(osmid, *node);
    }
}

struct Routing {
    int x,y;

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {
        //addNode(osmid, lon, lat, tags.find("place") != tags.end());
    }

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {

        if (tags.find("highway") != tags.end()) {
            for (uint64_t ref : refs) {
                addNode(ref, 0.0, 0.0, false);
            }
        }
    }

    void relation_callback (uint64_t /*osmid*/, const Tags &/*tags*/, const References & refs){}
};

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage: %s file_to_read.osm.pbf\n", argv[0]);
        return 1;
    }
    Routing routing;

    printf(
        "a rangeFail = we search a key in a file, but it does not exists in that file.\n"
        "     items,    in Ram,   updates, bloomSuccess, bloomFail, rangeSuccess, rangeFail, fileLoads,   used KB\n"
    );

    tipo0 = Clock::now();

    //read_osm_pbf(argv[1], routing, false); // read nodes and relations
    read_osm_pbf(argv[1], routing, true); // read way only

    return 0;
}

