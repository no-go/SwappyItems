#include <cstdio>
#include <utility> // pair
#include <chrono>

#include <inttypes.h>
#include <vector>
#include "SwappyItems.hpp"

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
    uint8_t _used;
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

int main(int argc, char** argv) {
    Key query = 103535041; // 185769837
    if (argc == 2) query = stol(argv[1]);
    start = std::chrono::high_resolution_clock::now();

    ways = new SwappyItemsWAYS(23);
    nodes = new SwappyItemsNODES(42);
    places = new SwappyItemsPLACES(815);
    
    // get a way by key -------------------------------------------
    
    std::pair<WayData, vector<Key> > * val = ways->get(query);
    
    if (val == nullptr) {
        printf("The '%lu' does not exist.\n", query);
    } else {
        printf("Name of '%lu': %s\n", query, val->first._name);
        printf("     uses: %d\n", val->first._used);
    }
    // ---------------------------------------------------------
    
    SwappyItemsPLACES::Data p;
    Key k2;
    bool here = places->each(p, [&k2](Key key, SwappyItemsPLACES::Data & val) {
        printf("each %d gets '%lu' %s\n", val.first._type, key, val.first._name);
        k2 = key; // an other kind of "returning" a value
        //val.first._type++; // change data works!!
        // stop on true
        return val.first._name[1] == 'o'; // the second letter must be a "o"
    });
    if (here) printf("found: '%lu' %s\n", k2, p.first._name);

    SwappyItemsWAYS::Data w;
    ways->each(w, [](Key key, SwappyItemsWAYS::Data & val) {
        if (val.first._name[0] == 'A') {
            printf("'%lu' with type %d has name %s\n", key, val.first._type, val.first._name);
            for (auto k : val.second) {
                printf("  ref '%lu'\n", k);
            }
        }
        // all items, no stop
        return false;
    });

    SwappyItemsNODES::Data n;
    nodes->each(n, [](Key key, SwappyItemsNODES::Data & val) {
        if (val.first._used > 2) {
            printf("%lu\n", key);
        }
        // all items, no stop
        return false;
    });
    
    delete places;
    delete ways;
    delete nodes;
    
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("# end: %.f ms\n", mseconds);
    return 0;
}

