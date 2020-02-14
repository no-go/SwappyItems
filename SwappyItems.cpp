#define __STDC_FORMAT_MACROS
#include <cstdlib>
#include <cstdio>
#include <utility> // pair

#include <inttypes.h>
#include <vector>

#include <stxxl/unordered_map>
#include "osmpbfreader.h"

using namespace CanalTP;
using namespace std;

//#define MAX_NODES     2405 814 605.0 // EUROPE
//#define MAX_NODES       67 853 945.0 // NRW (Node mit 18byte sollte dann 1,2 GB RAM brauchen)


#define FILE_ITEMS  32*1024
#define FILE_MULTI        4
#define RAM_MULTI         2

#define SUB_BLOCK_SIZE         32
#define SUB_BLOCKS_PER_BLOCK  (1024 * FILE_MULTI * RAM_MULTI)

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

struct HashFunctor {
    size_t operator () (Key key) const {
        return (size_t)(key * 2654435761u);
    }
};
struct CompareLess {
    bool operator () (const Key& a, const Key& b) const { return a < b; }
    static Key min_value() { return numeric_limits<Key>::min(); }
    static Key max_value() { return numeric_limits<Key>::max(); }
};

typedef stxxl::unordered_map<
 Key, Value, HashFunctor, CompareLess, SUB_BLOCK_SIZE, SUB_BLOCKS_PER_BLOCK
> KVstore;

// ---------------------------------------------------------------------

struct Routing {
    KVstore * ways;

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {}

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {
        if (tags.find("highway") != tags.end()) {
            for (uint64_t ref : refs) {
                KVstore::iterator wa = ways->find(ref);

                if (ways->size()%2048 == 0) {
                    printf(
                        "%10lld %10lld ",
                        ways->size(), ways->buffer_size()
                    );
                }

                if (wa == ways->end()) {
                    Value dummy;
                    ValueSet(dummy, 0.0, 0.0, false);
                    ways->insert(make_pair(ref, dummy));
                } else {
                    wa->second._uses++;
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
    KVstore * ways = new KVstore();
    Routing routing;
    routing.ways = ways;

    //read_osm_pbf(argv[1], routing, false); // initial read nodes (and relations)
    read_osm_pbf(argv[1], routing, true); // read way only

    return 0;
}

