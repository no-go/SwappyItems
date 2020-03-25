// we need it for PRId32 in snprintf
#define __STDC_FORMAT_MACROS

//#include <cstdlib>
#include <cstdio>
//#include <cstring>
#include <utility> // pair

#include <inttypes.h>
#include <vector>

#include "SwappyItems.hpp"
#include "osmpbfreader.hpp"

using namespace CanalTP;
using namespace std;

#define FILE_ITEMS           (  64*1024) // 2*1024
#define FILE_MULTI                   16  // 4
#define RAM_MULTI                     8  // 8
#define BBITS                         5  // 5
#define BMASK   ((BBITS+4)*  FILE_ITEMS) // +4

typedef uint64_t Key; // for the key-value tuple, 8 byte

struct NodeData {
    uint8_t _used;
    double _lon;
    double _lat;
};

typedef SwappyItems<Key, NodeData, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> SwappyItemsNODES;
SwappyItemsNODES * nodes;

void logEntry() {
    auto stat = nodes->getStatistic();
    printf(
        "%10ld i "
        "%10ld q "
        
        "%10" PRId64 " u "
        "%10" PRId64 " d "

        "%10" PRId64 " r "
        "%10" PRId64 " b "
        "%10" PRId64 " e "
        
        "%10" PRId64 " s "
        "%10" PRId64 " l "
        "%10ld filekB "

        "%10" PRId64 " "
        "%10" PRId64 "\n",
        
        stat.size,
        stat.queue,
        
        stat.updates,
        stat.deletes,
        
        stat.rangeSaysNo, 
        stat.bloomSaysNotIn,
        stat.rangeFails,
        
        stat.swaps,
        stat.fileLoads,
        stat.fileKB,
        
        stat.minKey,
        stat.maxKey
    );
}

struct Routing {

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {}
    
    void relation_callback (uint64_t osmid, const Tags & tags, const References & refs){}

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {
        pair<NodeData, vector<Key> > * nodeptr = nodes->get(osmid);
        
        if (osmid < 145849) printf("hurra!\n");
        
        if (nodeptr == nullptr) {
            NodeData nd;
            nd._used = 1;
            nd._lon = lon;
            nd._lat = lat;
            nodes->set(osmid, nd);
        } else {
            nodeptr->first._used++;
            if (lon != 0) nodeptr->first._lon = lon;
            if (lat != 0) nodeptr->first._lat = lat;
            nodes->set(osmid, nodeptr->first);
        }
        if ((nodes->getStatistic().size%4096 == 0)) logEntry();
    }
    
};

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage: %s file_to_read.osm.pbf\n", argv[0]);
        return 1;
    }

    nodes = new SwappyItemsNODES(42);
    Routing routing;

    read_osm_pbf(argv[1], routing, false); // read nodes and relations
    
    delete nodes;
    return 0;
}

