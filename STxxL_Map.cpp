#define __STDC_FORMAT_MACROS
#include <cstdlib>
#include <cstdio>
#include <utility> // pair
#include <ctime>

#include <inttypes.h>
#include <vector>

#include <stxxl/map>
#include "osmpbfreader.h"

using namespace CanalTP;
using namespace std;

//#define MAX_NODES     2405 814 605.0 // EUROPE
//#define MAX_NODES       67 853 945.0 // NRW (Node mit 18byte sollte dann 1,2 GB RAM brauchen)


#define FILE_ITEMS  32*1024
#define FILE_MULTI        4
#define RAM_MULTI         2

#define DATA_NODE_BLOCK_SIZE (4096)
#define DATA_LEAF_BLOCK_SIZE (4096)

typedef uint64_t Key; // for the key-value tuple, 8 byte

struct Value {
    bool _town;    // 1byte
    uint8_t _uses; // 1byte
    double _lon;   // 8byte
    double _lat;   // 8byte
    
    friend std::ostream & operator<<(std::ostream & out, Value const & val) {
        out << val._lon;
        return out;
    }
};

void ValueSet (Value & v, double lon = 0, double lat = 0, bool town = false, uint8_t uses = 0) {
    v._uses = uses;
    v._lon = lon;
    v._lat = lat;
    v._town = town;
}


bool stxxlIsEmpty = true;



struct CompareGreater {
    bool operator () (const Key& a, const Key& b) const { return a > b; }
    static Key min_value() { return numeric_limits<Key>::min(); }
    static Key max_value() { return numeric_limits<Key>::max(); }
};

typedef stxxl::map<
 Key, Value, CompareGreater, DATA_NODE_BLOCK_SIZE, DATA_LEAF_BLOCK_SIZE
> KVstore;

// ---------------------------------------------------------------------

time_t start;
double seconds;

struct Routing {
    KVstore * ways;

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {}

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {
        if (tags.find("highway") != tags.end()) {
            KVstore::iterator wa = ways->end();
            for (uint64_t ref : refs) {

                if (!stxxlIsEmpty) wa = ways->find(ref);

                if (ways->size()%2048 == 0) {
                    time_t now = time(nullptr);
                    seconds = difftime(now,start);
                    printf(
                        "%10.f "
                        "%10lld ",
                        seconds,
                        ways->size()
                    );
                    ways->print_statistics(std::cout);
                }
                
                if (wa == ways->end()) {
                    stxxlIsEmpty = false;
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
    
    stxxl::config * cfg = stxxl::config::get_instance();
    cfg->add_disk( stxxl::disk_config("disk=/tmp/stxxl-SwappyItems.tmp, 4 GiB,memory") );

    KVstore * ways = new KVstore(
        (KVstore::node_block_type::raw_size)*3,
        (KVstore::leaf_block_type::raw_size)*3
    );
    Routing routing;
    routing.ways = ways;
    
    start = time(nullptr);

    //read_osm_pbf(argv[1], routing, false); // initial read nodes (and relations)
    read_osm_pbf(argv[1], routing, true); // read way only
    
    time_t now = time(nullptr);
    seconds = difftime(now,start);
    printf("end: %.f\n", seconds);
    
    delete ways;
    return 0;
}

