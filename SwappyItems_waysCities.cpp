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
#include "osmpbfreader.h"

using namespace CanalTP;
using namespace std;

#define FILE_ITEMS           (   4*1024)
#define FILE_MULTI                    4
#define RAM_MULTI                     8
#define BBITS                         5
#define BMASK   ((BBITS+4)*  FILE_ITEMS)

typedef uint64_t Key; // for the key-value tuple, 8 byte

struct Value {
    uint8_t _town;
    uint8_t _uses;
    double _lon;
    double _lat;
    Key _last; // if we want to store the whole ref list, then hiberate and file swap must be modified for collection vars !
    char _name[256];
};

void ValueSet (Value & v, Key last = 0, double lon = 0.0, double lat = 0.0, uint8_t town = 0, uint8_t uses = 0) {
    v._uses = uses;
    v._lon = lon;
    v._lat = lat;
    v._town = town;
    v._last = last;
}

typedef SwappyItems<Key, Value, FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK> KVstore;
KVstore * ways;

// ---------------------------------------------------------------------

std::chrono::time_point<std::chrono::system_clock> start;
double mseconds;


string replacer(string& s, const string& toReplace, const string& replaceWith) {
    size_t pos = 0;
    bool endless = true;
    while(endless) {
        pos = s.find(toReplace, pos);
        if (pos == std::string::npos) {
            endless = false;
        } else {
            s = s.replace(pos, toReplace.length(), replaceWith);
        }
    }
    
    return s;
}

int parseLine(char* line) {
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}

int getUsedKB() {
    FILE* fi = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, fi) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(fi);
    return result;
}

atomic<bool> isPrinted = false;

struct Routing {
    KVstore * ways;

    void way_callback (uint64_t osmid, const Tags & tags, const vector<uint64_t> & refs) {
        if (tags.find("highway") != tags.end()) {
            Value * wa;
            
            // debug brake ----------------------------------
            //if (ways->size() == 1234567) { // @todo ????????????????????????? does not work!!!
            //    ways->hibernate();
            //    exit(1);
            //}
            // ----------------------------------------------
            
            string wayName = "";
            if(tags.find("name") != tags.end()){
                wayName = tags.at("name");
            } else if (tags.find("ref") != tags.end()) {
                wayName = tags.at("ref");
            } else if (tags.find("destination:ref") != tags.end() && tags.find("destination") != tags.end()) {
                wayName = tags.at("destination:ref") + (string) " Richtung " + tags.at("destination");
            }
            wayName = replacer(wayName, "&", "und");
            wayName = replacer(wayName, "\"", "'");
            wayName = replacer(wayName, "<", "");
            wayName = replacer(wayName, ">", "");
            
            for (size_t i=0; i < refs.size(); ++i) {

                wa = ways->get(refs[i]);

                if ((ways->size()%1024 == 0) && (isPrinted == false)) {
                    isPrinted = true;
                    auto now = std::chrono::high_resolution_clock::now();
                    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
                    printf(
                        "%10.f ms "
                        "%10ld i "
                        "%10ld q "
                        
                        "%10" PRId64 " u "
                        "%10" PRId64 " d "

                        "%10" PRId64 " r "
                        "%10" PRId64 " b "
                        "%10" PRId64 " e "
                        
                        "%10" PRId64 " s "
                        "%10" PRId64 " l "
                        "%10d kB "
                        "%10ld filekB\n",
                        
                        mseconds,
                        ways->size(),
                        ways->prioSize(),
                        
                        ways->statistic.updates,
                        ways->statistic.deletes,
                        
                        ways->statistic.rangeSaysNo, 
                        ways->statistic.bloomSaysFresh,
                        ways->statistic.rangeFails,
                        
                        ways->statistic.swaps,
                        ways->statistic.fileLoads,
                        getUsedKB(),
                        ways->size(true)
                    );
                }
                
                Value dummy;
                if (wa == nullptr) {
                    ValueSet(dummy, (i==0 ? osmid : refs[i-1]) );
                    snprintf(dummy._name, 256, "%s", wayName.c_str());
                    // prevent a log print every second, if size not changes
                    isPrinted = false;
                    
                } else {
                    ValueSet(dummy, wa->_last, wa->_lon, wa->_lat, wa->_town, wa->_uses+1);
                }
                ways->set(refs[i], dummy);
            }
        }
    }

    void node_callback (uint64_t osmid, double lon, double lat, const Tags & tags) {
        Value * wa = ways->get(osmid);
        Value dummy;
        
        if (wa == nullptr) {
            // it seams to be not a way: we store it as town?
            
            if (tags.find("place") != tags.end()) {
                string pt = tags.at("place");
                uint8_t relevantTown = 1;
                
                if (pt.compare("city") == 0) {
                    relevantTown = 4;
                } else if (pt.compare("town") == 0) {
                    relevantTown = 3;
                } else if (pt.compare("village") == 0) {
                    relevantTown = 2;
                }
                ValueSet(dummy, osmid, lon, lat, relevantTown);
                if (tags.find("name") != tags.end()) {
                    pt = tags.at("name");
                    pt = replacer(pt, "&", "und");
                    pt = replacer(pt, "\"", "'");
                    pt = replacer(pt, "<", "");
                    pt = replacer(pt, ">", "");
                } else {
                    pt = (string) "Dingenskirchen";
                }
                snprintf(dummy._name, 256, "%s", pt.c_str());
                ways->set(osmid, dummy);
            }
        } else {
            // this osmid node is a way (set lon and lat!)
            ValueSet(dummy, wa->_last, lon, lat, wa->_town, wa->_uses);
            ways->set(osmid, dummy);
            isPrinted = false;
        }

        if ((ways->statistic.updates%1024 == 0) && (isPrinted == false)) {
            isPrinted = true;
            auto now = std::chrono::high_resolution_clock::now();
            mseconds = std::chrono::duration<double, std::milli>(now-start).count();
            printf(
                "%10.f ms "
                "%10ld i "
                "%10ld q "
                
                "%10" PRId64 " u "
                "%10" PRId64 " d "

                "%10" PRId64 " r "
                "%10" PRId64 " b "
                "%10" PRId64 " e "
                
                "%10" PRId64 " s "
                "%10" PRId64 " l "
                "%10d kB "
                "%10ld filekB\n",
                
                mseconds,
                ways->size(),
                ways->prioSize(),
                
                ways->statistic.updates,
                ways->statistic.deletes,
                
                ways->statistic.rangeSaysNo, 
                ways->statistic.bloomSaysFresh,
                ways->statistic.rangeFails,
                
                ways->statistic.swaps,
                ways->statistic.fileLoads,
                getUsedKB(),
                ways->size(true)
            );
        }
    }
    
    void relation_callback (uint64_t /*osmid*/, const Tags &/*tags*/, const References & refs){}
};


void my_handler(int s) {
    printf("#Caught signal %d\n", s);
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("#end (before hibernate): %.f ms\n", mseconds);
    ways->hibernate();
    exit(1);
}

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Usage: %s file_to_read.osm.pbf\n", argv[0]);
        return 1;
    }
    
    ways = new KVstore(23);
    Routing routing;
    routing.ways = ways;
    
    printf("# cores:                  %12d\n", thread::hardware_concurrency());
    printf("# items in a single file: %12d\n", FILE_ITEMS);
    printf("# swap if more than       %12d items in RAM are reached\n",  RAM_MULTI * FILE_MULTI * FILE_ITEMS);
    printf("# swap into               %12d files\n", FILE_MULTI);
    printf("# use                     %12d Bloom bits for a key in a file bitmask\n", BBITS);
    printf("# use a bitmask with      %12d bits for each file\n", BMASK);

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    start = std::chrono::high_resolution_clock::now();

    read_osm_pbf(argv[1], routing, true); // read way only
    
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("# end ways: %.f ms\n", mseconds);

    read_osm_pbf(argv[1], routing, false); // initial read nodes (and relations)
    
    now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("# end nodes: %.f ms\n", mseconds);
    
    // stores final data structure!
    ways->hibernate();
    
    delete ways;
    return 0;
}

