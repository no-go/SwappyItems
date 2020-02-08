// try to store many nodes info beyond the RAM !?!

// g++ SwappyItems.cpp -Wall -O2 -mtune=native -lz -losmpbf -lprotobuf -std=c++11 -o SwappyItems.exe
// g++ %f -Wall -O2 -mtune=native -std=c++11 libz.a libosmpbf.a libprotobuf.a -o %e.exe

#define __STDC_FORMAT_MACROS
#include <cstdlib>
#include <cstdio>
#include <iostream>    // sizeof
#include <cstring>
#include <ctime>

#include <inttypes.h>
#include <vector>

#include "SwappyItems.hpp"
#include "osmpbfreader.h"

using namespace CanalTP;
using namespace std;

//#define MAX_NODES     2405 814 605.0 // EUROPE
//#define MAX_NODES       67 853 945.0 // NRW (Node mit 18byte sollte dann 1,2 GB RAM brauchen)


#define FILE_ITEMS  32*1024
#define FILE_MULTI        4
#define RAM_MULTI         2

typedef uint64_t Key; // for the key-value tuple

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

typedef SwappyItems<Key,Value,(FILE_ITEMS),FILE_MULTI,RAM_MULTI> KVstore;

// ---------------------------------------------------------------------

FILE * pFileLog2;
FILE * pFileLog3;
time_t start;
double seconds;

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
    int x,y;
    KVstore * ways;
    KVstore * nodes;

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {
        Value dummy;
        ValueSet(dummy, lon, lat, tags.find("place") != tags.end());
        nodes->set(osmid, dummy);

        if (nodes->size()%2048 == 0) {
            time_t now = time(nullptr);
            seconds = difftime(now,start);

            fprintf(pFileLog2,
                "%10.f "
                "%10" PRId64 " %10" PRId64 " "
                "%10" PRId64 " "

                "%10" PRId64 " "
                "%13" PRId64 " "
                "%10" PRId64 " "
                "%13" PRId64 " "
                "%10" PRId64 " "
                "%10" PRId64 " "
                "%10d\n",
                seconds,
                nodes->size(), nodes->size(true),
                nodes->prioSize(),

                nodes->statistic.updates,
                nodes->statistic.bloomSaysFresh,
                nodes->statistic.bloomFails,
                nodes->statistic.rangeSaysNo, nodes->statistic.rangeFails, nodes->statistic.fileLoads,
                getUsedKB()
            );
            fflush(pFileLog2);
        }
    }


    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {
        if (tags.find("highway") != tags.end()) {
            for (uint64_t ref : refs) {
                Value * wa = ways->get(ref);

                if (ways->size()%2048 == 0) {
                    time_t now = time(nullptr);
                    seconds = difftime(now,start);

                    fprintf(pFileLog3,
                        "%10.f "
                        "%10" PRId64 " %10" PRId64 " "
                        "%10" PRId64 " "

                        "%10" PRId64 " "
                        "%13" PRId64 " "
                        "%10" PRId64 " "
                        "%13" PRId64 " "
                        "%10" PRId64 " "
                        "%10" PRId64 " "
                        "%10d\n",
                        seconds,
                        ways->size(), ways->size(true),
                        ways->prioSize(),

                        ways->statistic.updates,
                        ways->statistic.bloomSaysFresh,
                        ways->statistic.bloomFails,
                        ways->statistic.rangeSaysNo, ways->statistic.rangeFails, ways->statistic.fileLoads,
                        getUsedKB()
                    );
                    fflush(pFileLog3);
                }

                if (wa == nullptr) {
                    // try to get it from the nodes
                    Value * node = nodes->get(ref);
                    Value dummy;

                    if (node == nullptr) {
                        // this is not normal. reading nodes in the first step must get this ref!
                        ValueSet(dummy, 0.0, 0.0, false);
                    } else {
                        // logic: town and highway? false
                        // uses? inital 0
                        ValueSet(dummy, node->_lon, node->_lat, node->_town, node->_uses);
                    }

                    ways->set(ref, dummy);
                } else {
                    //printf("%" PRId64 " reused\n", osmid);
                    wa->_uses++;
                    ways->set(ref, *wa);
                }
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
    KVstore * nodes = new KVstore(2);
    KVstore * ways  = new KVstore(3);
    Routing routing;
    routing.nodes = nodes;
    routing.ways  = ways;

    start = time(nullptr);

    pFileLog2 = fopen("./filling2.log", "w");
    pFileLog3 = fopen("./filling3.log", "w");

    fprintf(pFileLog3, "file:       %s\n", argv[1]);
    fprintf(pFileLog3, "key size:   %10ld Bytes\n", sizeof(Key));
    fprintf(pFileLog3, "value size: %10ld Bytes\n", sizeof(Value));
    fprintf(pFileLog3, "RAM:        %10d items\n", (FILE_ITEMS*FILE_MULTI*RAM_MULTI) );
    fprintf(pFileLog3, "each swap:  %10d files\n", FILE_MULTI );
    fprintf(pFileLog3, "each file:  %10d items\n", FILE_ITEMS );

    fprintf(pFileLog3, "a rangeFail = we search a key in a file, but it does not exists in that file.\n");
    fprintf(pFileLog3,
        "       now,     items,    in Ram,  in Deque,   updates, bloomSuccess, bloomFail, rangeSuccess, rangeFail, fileLoads,   used KB\n"
    );

    read_osm_pbf(argv[1], routing, false); // initial read nodes (and relations)
    read_osm_pbf(argv[1], routing, true); // read way only

    fclose(pFileLog3);
    fclose(pFileLog2);

    time_t now = time(nullptr);
    seconds = difftime(now,start);
    printf("end: %.f\n", seconds);
    return 0;
}

