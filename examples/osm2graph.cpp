#include <cstdio>
#include <utility> // pair
#include <inttypes.h>
#include <vector>
#include <cstring>
#include <cmath> // acos
#include <algorithm> // reverse
#include "SwappyItems.hpp"

#include "osmpbfreader.hpp"
using namespace CanalTP;

#define FILE_ITEMS           (   7*1024) // 2*1024  // 64*1024
#define FILE_MULTI                    6  // 4       // 16
#define RAM_MULTI                     9  // 8
#define BBITS                         5  // 5
#define BMASK   ((BBITS+4)*  FILE_ITEMS) // +4

#define TORADI(angleDegrees) ((angleDegrees) * M_PI / 180.0)


// europa: lon  -10..25
//         lat   35..70
//#define LON_BOUND -10.0
//#define LON_BOUND_DIF 35.0
//#define LAT_BOUND 35.0
//#define LAT_BOUND_DIF 35.0

// nrw:    lon   5.85..9.47   like x (0=London, 90=Bhutan)
//         lat  50.32..52.52  like y (0=Equartor, 90=Nothpole)
#define LON_BOUND 5.85
#define LON_BOUND_DIF 3.62
#define LAT_BOUND 50.32
#define LAT_BOUND_DIF 2.2

typedef uint64_t Key; // for the key-value tuple, 8 byte

// --------------------------------------------------- OSM DATA

// + id = osmid of a osm node
struct NodeData {
    uint8_t _used;
    double _lon;
    double _lat;
};

// + id = osmid of a osm way
struct WayData {
    bool _oneway; // store refs in reverse order, if oneway is -1
};
// + list of refs to osm nodes (path)


// --------------------------------------------------- RESULT DATA

// + id = osmid of a osm node
struct Vertex {
    Key _way; // note: the first seen osmid of the way, where we see this node
    double _lon;
    double _lat;
};
// + list of refs to other Vertex

// + id = osmid of a osm node
struct Distance {
    uint8_t _dummy;
};
// + list of refs to store distances

typedef SwappyItems<Key, WayData, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> Ways_t;
typedef SwappyItems<Key, NodeData, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> Nodes_t;
Ways_t * ways;
Nodes_t * nodes;


typedef SwappyItems<Key, Vertex, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> Vertices_t;
Vertices_t * verticies;
typedef SwappyItems<Key, Distance, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> Distance_t;
Distance_t * distances;

uint64_t calcDist(double lon1, double lat1, double lon2, double lat2) {
    double dist = 6378388.0 * std::acos(std::sin(TORADI(lat1)) * std::sin(TORADI(lat2)) + std::cos(TORADI(lat1)) * std::cos(TORADI(lat2)) * std::cos(TORADI(lon2 - lon1)));
    if (dist < 0) dist *=-1;
    return dist;
}

uint8_t relevance(const std::string & wt) {
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

    void node_callback(uint64_t osmid, double lon, double lat, const Tags &/*tags*/) {
        // not valid pos
        if (lon == 0 && lat == 0) return;
        /// @todo not part of my area
        double checklon = lon - LON_BOUND;
        double checklat = lat - LAT_BOUND;
        if (checklon > LON_BOUND_DIF || checklon < 0.0) return;
        if (checklat > LAT_BOUND_DIF || checklat < 0.0) return;
        
        Nodes_t::Data * nodeptr = nodes->get(osmid);
        // not a node of my ways (I read node ids from ways first
        if (nodeptr == nullptr) return;
        /// @todo instead of lat,lon store the complete distance SOMEWHERE
        // we only use nodes of connected ways? (ignore sackgasse and links in between -> fail on distance!)
        if (nodeptr->first._used == 0) return;
        nodeptr->first._lon = lon;
        nodeptr->first._lat = lat;
        nodes->set(osmid, nodeptr->first);
    }

    void way_callback(uint64_t osmid, const Tags &tags, const std::vector<uint64_t> &refs) {
        
        if (tags.find("highway") != tags.end()) {
            uint8_t wt = relevance(tags.at("highway"));
            if (wt == 0) return;
            
            std::vector<Key> path;
            WayData way;
            unsigned pathlength = refs.size();
            way._oneway = false;
            bool rev = false;
            bool circle = false;
            
            if (wt == 2) way._oneway = true; // it is a link! oneway=yes is a default
            
            if (tags.find("oneway") != tags.end()) {
                std::string wt = tags.at("oneway"); // yes, no, reversible, -1, ...
                if (wt.compare("yes") == 0) way._oneway = true;
                if (wt.compare("no") == 0) way._oneway = false;
                if (wt.compare("-1") == 0) {
                    way._oneway = true;
                    rev = true;
                }
            }
            if (pathlength>1) {
                if (refs[0] == refs[pathlength-1]) circle = true;
            }
            
            Nodes_t::Data * nodeptr;
            if (circle) {
                for (unsigned i=1; i < pathlength; ++i) {
                    nodeptr = nodes->get(refs[i]);
                    if (nodeptr == nullptr) {
                        Nodes_t::Data node;
                        node.first._used = 0;
                        node.first._lon = 0.0;
                        node.first._lat = 0.0;
                        nodes->set(refs[i], node.first);
                    } else {
                        // node is also used in another way!
                        nodeptr->first._used++;
                        nodes->set(refs[i], nodeptr->first);
                    }
                    path.push_back(refs[i]);
                }
                path.push_back(refs[1]); // add the real firts node additionally to the end
            } else {
                for (unsigned i=0; i < pathlength; ++i) {
                    nodeptr = nodes->get(refs[i]);
                    if (nodeptr == nullptr) {
                        Nodes_t::Data node;
                        node.first._used = 0;
                        node.first._lon = 0.0;
                        node.first._lat = 0.0;
                        nodes->set(refs[i], node.first);
                    } else {
                        // node is also used in another way!
                        nodeptr->first._used++;
                        nodes->set(refs[i], nodeptr->first);
                    }
                    path.push_back(refs[i]);
                }
            }
            
            if (rev) std::reverse(path.begin(), path.end());
            ways->set(osmid, way, path);
        }
    }

    // today We don't care about relations
    void relation_callback(uint64_t /*osmid*/, const Tags &/*tags*/, const References & /*refs*/){}
};

int main(int argc, char** argv) {
    Routing routing;
    ways = new Ways_t(1);
    nodes = new Nodes_t(2);
    
    verticies = new Vertices_t(3);
    distances = new Distance_t(4);
    
    read_osm_pbf(argv[1], routing, true); // ways

    read_osm_pbf(argv[1], routing, false); // nodes und bedingungen
    
    // create verticies -------------------------------------------------------------------------------------
    
    Ways_t::Data w;
    ways->each(w, [](Key wayosmid, Ways_t::Data & way) {
        bool firstItem = true;
        Key last;
        double lastLon = 0.0;
        double lastLat = 0.0;

        // create verticies from refs: a->b->c->d->e
        // d link to e(=last)
        // c link to d(=last)
        // ...
        for (unsigned i=0; i < way.second.size(); ++i) {
            Key ref = way.second[way.second.size()-1 - i]; // add actually ref to the last ref (from backward)
            Nodes_t::Data * nptr = nodes->get(ref);
            
            if (nptr == nullptr) continue;
            if (nptr->first._used == 0) continue;
            if (nptr->first._lon == 0 && nptr->first._lat == 0) continue;
            
            Vertices_t::Data * vptr = verticies->get(ref);
            Distance_t::Data * dptr = distances->get(ref);
            
            if (firstItem) {
                firstItem = false;
                if (vptr == nullptr) {
                    // create e
                    Vertices_t::Data vertex;
                    Distance_t::Data distance;
                    vertex.first._way = wayosmid;
                    vertex.first._lon = nptr->first._lon;
                    vertex.first._lat = nptr->first._lat;
                    lastLon = vertex.first._lon;
                    lastLat = vertex.first._lat;
                    verticies->set(ref, vertex.first, vertex.second);
                    distances->set(ref, distance.first, distance.second); // empty dummy
                }
            } else {
                if (vptr == nullptr) {
                    Vertices_t::Data vertex;
                    Distance_t::Data distance;
                    vertex.first._way = wayosmid;
                    vertex.first._lon = nptr->first._lon;
                    vertex.first._lat = nptr->first._lat;
                    vertex.second.push_back(last);
                    uint64_t dist = calcDist(vertex.first._lon, vertex.first._lat, lastLon, lastLat);
                    distance.second.push_back(dist);
                    lastLon = vertex.first._lon;
                    lastLat = vertex.first._lat;
                    verticies->set(ref, vertex.first, vertex.second);
                    distances->set(ref, distance.first, distance.second);
                } else {
                    vptr->second.push_back(last);
                    uint64_t dist = calcDist(vptr->first._lon, vptr->first._lat, lastLon, lastLat);
                    dptr->second.push_back(dist);
                    lastLon = vptr->first._lon;
                    lastLat = vptr->first._lat;
                    verticies->set(ref, vptr->first, vptr->second);
                    distances->set(ref, dptr->first, dptr->second);
                }
            }
            last = ref;
        }
        
        // Not a oneway. create verticies from refs in the direction: a<-b<-c<-d<-e
        // b link to a(=last)
        // c link to b(=last)
        // ...
        firstItem = true;
        if (way.first._oneway == false) {
            for (unsigned i=0; i < way.second.size(); ++i) {
                Key ref = way.second[i];
                Nodes_t::Data * nptr = nodes->get(ref);
                
                if (nptr == nullptr) continue;
                if (nptr->first._used == 0) continue;
                if (nptr->first._lon == 0 && nptr->first._lat == 0) continue;
                
                Vertices_t::Data * vptr = verticies->get(ref);
                Distance_t::Data * dptr = distances->get(ref);

                if (vptr == nullptr) {
                    Vertices_t::Data vertex;
                    Distance_t::Data distance;
                    vertex.first._way = wayosmid;
                    vertex.first._lon = nptr->first._lon;
                    vertex.first._lat = nptr->first._lat;
                    if (firstItem) {
                        firstItem = false;
                    } else {
                        uint64_t dist = calcDist(vertex.first._lon, vertex.first._lat, lastLon, lastLat);
                        vertex.second.push_back(last);
                        distance.second.push_back(dist);
                    }
                    verticies->set(ref, vertex.first, vertex.second);
                    distances->set(ref, distance.first, distance.second);
                    lastLon = vertex.first._lon;
                    lastLat = vertex.first._lat;
                } else {
                    if (firstItem) {
                        firstItem = false;
                    } else {
                        uint64_t dist = calcDist(vptr->first._lon, vptr->first._lat, lastLon, lastLat);
                        vptr->second.push_back(last);
                        dptr->second.push_back(dist);
                    }
                    verticies->set(ref, vptr->first, vptr->second);
                    distances->set(ref, dptr->first, dptr->second);
                    lastLon = vptr->first._lon;
                    lastLat = vptr->first._lat;
                }
                last = ref;
            }
        }
        // all items, no stop
        return false;
    });
    
    // print verticies ------------------------------------------------
    
    Vertices_t::Data dummy;
    verticies->each(dummy, [](Key id, Vertices_t::Data & v) {
        printf("%ld (%f,%f) part of way %ld\n", id, v.first._lon, v.first._lat, v.first._way);
        for (Key r : v.second) {
            printf(" %" PRId64, r);
        }
        printf("\n");
        Distance_t::Data * dptr = distances->get(id);
        for (uint64_t r : dptr->second) {
            printf(" %" PRId64, r);
        }
        printf("\n");
        // all items, no stop
        return false;
    });
    

    delete verticies;
    delete nodes;
    delete ways;
    
    return 0;
}

