// we need it for PRId32 in snprintf
#define __STDC_FORMAT_MACROS

#include <cstdlib>
#include <cstdio>
#include <utility> // pair
#include <chrono>

#include <inttypes.h>
#include <vector>

#include "SwappyItems.hpp"
#include "osmpbfreader.h"

using namespace CanalTP;
using namespace std;

#define FILE_ITEMS    (  2*1024)
#define FILE_MULTI            8
#define RAM_MULTI            64
#define BBITS                 5
#define BMASK     (10*   2*1024)

typedef uint64_t Key; // for the key-value tuple, 8 byte

struct Value {
    bool _town;    // 1byte
    uint8_t _uses; // 1byte
    double _lon;   // 8byte
    double _lat;   // 8byte
};

void ValueSet (Value & v, double lon = 0, double lat = 0, bool town = false, uint8_t uses = 0) {
    v._uses = uses;
    v._lon = lon;
    v._lat = lat;
    v._town = town;
}

typedef SwappyItems<Key, Value, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> KVstore;

// ---------------------------------------------------------------------

std::chrono::time_point<std::chrono::system_clock> start;
double mseconds;

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
    FILE* fi = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, fi) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(fi);
    return result;
}

struct Routing {
    KVstore * ways;

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {}

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {
        if (tags.find("highway") != tags.end()) {
            Value * wa;
            for (uint64_t ref : refs) {

                wa = ways->get(ref);

                if (ways->size()%1024 == 0) {
                    auto now = std::chrono::high_resolution_clock::now();
                    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
                    printf(
                        "%10.f s "
                        "%10ld i "
                        "%10ld q "
                        
                        "%10" PRId64 " u "

                        "%10" PRId64 " r "
                        "%10" PRId64 " b "
                        "%10" PRId64 " e "
                        
                        "%10" PRId64 " s "
                        "%10" PRId64 " f "
                        "%10d kB\n",
                        
                        mseconds/1000,
                        ways->size(),
                        ways->prioSize(),
                        
                        ways->statistic.updates,
                        
                        ways->statistic.rangeSaysNo, 
                        ways->statistic.bloomSaysFresh,
                        ways->statistic.rangeFails,
                        
                        ways->statistic.swaps,
                        ways->statistic.fileLoads,
                        getUsedKB()
                    );
                }
                Value dummy;
                if (wa == nullptr) {
                    
                    ValueSet(dummy, 0.0, 0.0, false);
                    
                } else {
                    ValueSet(dummy, wa->_lon, wa->_lat, wa->_town, wa->_uses+1);
                }
                ways->set(ref, dummy);
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
    
    KVstore * ways = new KVstore(23);
    Routing routing;
    routing.ways = ways;
    
    start = std::chrono::high_resolution_clock::now();

    //read_osm_pbf(argv[1], routing, false); // initial read nodes (and relations)
    read_osm_pbf(argv[1], routing, true); // read way only
    
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("end: %.f ms\n", mseconds);
    
    delete ways;
    return 0;
}

