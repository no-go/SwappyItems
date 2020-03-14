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
#include "osmpbfreader.hpp"

using namespace CanalTP;
using namespace std;

#define FILE_ITEMS           (   2*1024)
#define FILE_MULTI                    4
#define RAM_MULTI                     8
#define BBITS                         5
#define BMASK   ((BBITS+4)*  FILE_ITEMS)

typedef uint64_t Key; // for the key-value tuple, 8 byte

struct NodeData {
    uint8_t _used;
    double _lon;
    double _lat;
};

struct WayData {
    uint8_t _type;
    char _name[256];
};

struct PlaceData {
    uint8_t _type;
    double _lon;
    double _lat;
    char _name[256];
};

typedef SwappyItems<Key, WayData, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> SwappyItemsWAYS;
typedef SwappyItems<Key, NodeData, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> SwappyItemsNODES;
typedef SwappyItems<Key, PlaceData, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> SwappyItemsPLACES;
SwappyItemsWAYS * ways;
SwappyItemsNODES * nodes;
SwappyItemsPLACES * places;

// ---------------------------------------------------------------------

std::chrono::time_point<std::chrono::system_clock> start;
double mseconds;
atomic<bool> isPrinted = false;

#include "tools.hpp"
#include "Routing.hpp"

/** @example ReadPbfData.cpp
 * One of the main focus in SwappyItems is: processing big data like
 * pbf or osm files from open streetmaps. */
 
int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage: %s file_to_read.osm.pbf\n", argv[0]);
        return 1;
    }
    
    ways = new SwappyItemsWAYS(23);
    nodes = new SwappyItemsNODES(42);
    places = new SwappyItemsPLACES(815);
    Routing routing;
    
    logHead();
    catchSig();

    start = std::chrono::high_resolution_clock::now();

    read_osm_pbf(argv[1], routing, true); // read way only
    
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("# end ways: %.f ms\n", mseconds);

    read_osm_pbf(argv[1], routing, false); // read nodes and relations
    
    now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("# end nodes: %.f ms\n", mseconds);

    delete places;
    delete nodes;
    delete ways;
    return 0;
}

