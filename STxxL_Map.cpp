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

#define DATA_NODE_BLOCK_SIZE (4096)
#define DATA_LEAF_BLOCK_SIZE (4096)

struct CompareGreater {
 bool operator () (const uint64_t& a, const uint64_t& b) const
 { return a > b; }
 static uint64_t max_value()
 { return std::numeric_limits<uint64_t>::min(); }
};

typedef stxxl::map<uint64_t, bool   , CompareGreater, DATA_NODE_BLOCK_SIZE, DATA_LEAF_BLOCK_SIZE> STXXLtown;
typedef stxxl::map<uint64_t, uint8_t, CompareGreater, DATA_NODE_BLOCK_SIZE, DATA_LEAF_BLOCK_SIZE> STXXLuses;
typedef stxxl::map<uint64_t, double , CompareGreater, DATA_NODE_BLOCK_SIZE, DATA_LEAF_BLOCK_SIZE> STXXL_lon;
typedef stxxl::map<uint64_t, double , CompareGreater, DATA_NODE_BLOCK_SIZE, DATA_LEAF_BLOCK_SIZE> STXXL_lat;

STXXLtown _town((STXXLtown::node_block_type::raw_size) * 3, (STXXLtown::leaf_block_type::raw_size) * 3);
STXXLuses _uses((STXXLuses::node_block_type::raw_size) * 3, (STXXLuses::leaf_block_type::raw_size) * 3);
STXXL_lon _lon((STXXL_lon::node_block_type::raw_size) * 3, (STXXL_lon::leaf_block_type::raw_size) * 3);
STXXL_lat _lat((STXXL_lat::node_block_type::raw_size) * 3, (STXXL_lat::leaf_block_type::raw_size) * 3);

// ---------------------------------------------------------------------

time_t start;
double seconds;

struct Routing {

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {}

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {
        if (tags.find("highway") != tags.end()) {
            
            for (uint64_t ref : refs) {

                STXXLuses::iterator wa = _uses.find(ref);

                if (_uses.size()%2048 == 0) {
                    time_t now = time(nullptr);
                    seconds = difftime(now,start);
                    printf(
                        "%10.f "
                        "%10lld \n",
                        seconds,
                        _uses.size()
                    );
                    //_uses.print_statistics(std::cout);
                }
                
                if (wa == _uses.end()) {
                    _town.insert(make_pair(ref, false));
                    _uses.insert(make_pair(ref, 0));
                    _lon.insert(make_pair(ref, 0.0));
                    _lat.insert(make_pair(ref, 0.0));
                    
                } else {
                    //printf(".");
                    (wa->second)++;
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

    Routing routing;
                    
    start = time(nullptr);

    //read_osm_pbf(argv[1], routing, false); // initial read nodes (and relations)
    read_osm_pbf(argv[1], routing, true); // read way only
    
    time_t now = time(nullptr);
    seconds = difftime(now,start);
    printf("end: %.f\n", seconds);
    
    return 0;
}

