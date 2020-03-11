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

struct WayData {
    uint8_t _used;
    uint8_t _type;
    char _name[256];
};

typedef SwappyItems<Key, WayData, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> SwappyItemsWAYS;
SwappyItemsWAYS * ways;


// ---------------------------------------------------------------------

std::chrono::time_point<std::chrono::system_clock> start;
double mseconds;

int main(int argc, char** argv) {
    Key query = 103535041; // 185769837
    if (argc == 2) query = stol(argv[1]);
    start = std::chrono::high_resolution_clock::now();

    SwappyItemsWAYS * ways = new SwappyItemsWAYS(23);
    std::pair<WayData, vector<Key> > * val = ways->get(query);
    
    if (val == nullptr) {
        printf("The '%lu' does not exist.\n", query);
    } else {
        printf("Name of '%lu': %s\n", query, val->first._name);
        printf("     uses: %d\n", val->first._used);
    }
    
    delete ways;
    
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("# end: %.f ms\n", mseconds);
    return 0;
}

