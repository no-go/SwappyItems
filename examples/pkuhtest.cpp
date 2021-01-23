#include <cstdio>
#include <inttypes.h>
#include <vector>
#include <cstring>
#include "Pkuh.hpp"

// Pkuh ist nun anders! dieser code wird nicht mehr tun!

#include "osmpbfreader.hpp"
using namespace CanalTP;

Pkuh<> * pkuh;

int relevance(const std::string & wt) {
    if (wt.compare("motorway") == 0) return 1;
    if (wt.compare("motorway_link") == 0) return 2;
    if (wt.compare("trunk") == 0) return 1;
    if (wt.compare("trunk_link") == 0) return 2;
    if (wt.compare("primary") == 0) return 1;
    if (wt.compare("primary_link") == 0) return 2;
    if (wt.compare("secondary") == 0) return 1;
    if (wt.compare("secondary_link") == 0) return 2;
    if (wt.compare("unclassified") == 0) return 1;
    if (wt.compare("tertiary") == 0) return 1;
    if (wt.compare("tertiary_link") == 0) return 2;
    if (wt.compare("residential") == 0) return 1;
    return 0;
}

struct Routing {

    void way_callback(uint64_t osmid, const Tags &tags, const std::vector<uint64_t> &refs) {
        
        if (tags.find("highway") != tags.end()) {
            int wt = relevance(tags.at("highway"));
            if (wt == 0) return;
            
            for (unsigned i=0; i < refs.size(); ++i) {
                Pkuh<>::Element * element;
                element = pkuh->get(refs[i]);
                if (element == nullptr) {
                    Pkuh<>::Element e;
                    e.key = refs[i];
                    e.prio = 1;
                    e.successor = e.key;
                    if (i>0) e.successor = refs[i-1];
                    pkuh->set(e);
                } else {
                    element->prio++;
                }
            }
        }
    }

    // today We don't care about
    void relation_callback(uint64_t /*osmid*/, const Tags &/*tags*/, const References & /*refs*/){}
    
    void node_callback(uint64_t /*osmid*/, double /*lon*/, double /*lat*/, const Tags &/*tags*/) {}
};

int main(int argc, char** argv) {
    Routing routing;
    pkuh = new Pkuh();
    
    read_osm_pbf(argv[1], routing, true); // ways
    
    return 0;
}

