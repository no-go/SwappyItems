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

std::chrono::time_point<std::chrono::system_clock> start;
double mseconds;

int main(int argc, char** argv) {
    Key query = 313617373; // 428124430 // 250072529
    if (argc == 2) query = stol(argv[1]);
    start = std::chrono::high_resolution_clock::now();

    KVstore * ways = new KVstore(23);
    std::pair<Value, vector<Key> > * val = ways->get(query);
    
    if (val == nullptr) {
        printf("The '%lu' does not exist.\n", query);
    } else {
        printf("Name of '%lu': %s\n", query, val->first._name);
        printf("  lon,lat: %lf, %lf\n", val->first._lon, val->first._lat);
        printf("     uses: %d\n", val->first._uses);
    }
    
    delete ways;
    
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("# end: %.f ms\n", mseconds);
    return 0;
}

