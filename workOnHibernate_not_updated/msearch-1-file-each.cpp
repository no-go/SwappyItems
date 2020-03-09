// we need it for PRId32 in snprintf
#define __STDC_FORMAT_MACROS

#include <cstdio>
#include <cstdlib>     // srand, rand, exit
#include <cstring>
#include <chrono>

#include <inttypes.h>  // uintX_t stuff
#include <iostream>    // sizeof and ios namespace
#include <algorithm>   // find
#include <stdexcept>   // std::out_of_range

#include <vector>         // store file information
#include <set>            // a set of candidate files
#include <unordered_map>  // for a key->data

#include <fstream>        // file operations

#include <execution> // c++17 and you need  -ltbb
#include <mutex>     // for std::lock_guard and mutex
#include <thread>    // you need -lpthread
#include <atomic>

using namespace std;

#define FILE_ITEMS           (   4*1024)
#define FILE_MULTI                    4
#define RAM_MULTI                     8
#define BBITS                         5
#define BMASK   ((BBITS+4)*  FILE_ITEMS)

typedef uint64_t Key; // for the key-value tuple, 8 byte
typedef uint32_t  Id;

struct Value {
    uint8_t _town;
    uint8_t _uses;
    double _lon;
    double _lat;
    Key _last; // if we want to store the whole ref list, then hiberate and file swap must be modified for collection vars !
    char _name[256];
};

struct Detail {
    Key minimum;
    Key maximum;
    vector<bool> bloomMask;
    Id fid;
};
    
chrono::time_point<chrono::system_clock>    start;
unordered_map<Key,Value>                    _ramList;
vector<Detail>                              _ranges;
double                                      mseconds;
char                                        _swappypath[256];

void wakeup () {
    char filename[512];
    Key loadedKey;
    Value loadedValue;
    ifstream file;
    Detail detail;
    char cbit;
    Id length;

    snprintf(filename, 512, "%s/hibernate.ramlist", _swappypath);
    file.open(filename, ios::in | ios::binary);
    file.read((char *) &length, sizeof(Id));
    for (Id c = 0; c < length; ++c) {
        file.read((char *) &loadedKey, sizeof(Key));
        file.read((char *) &loadedValue, sizeof(Value));
        _ramList[loadedKey] = loadedValue;
    }
    file.close();
    
    snprintf(filename, 512, "%s/hibernate.ranges", _swappypath);
    file.open(filename, ios::in | ios::binary);
    file.read((char *) &length, sizeof(Id));

    for (Id c = 0; c < length; ++c) {
        file.read((char *) &(detail.minimum), sizeof(Key));
        file.read((char *) &(detail.maximum), sizeof(Key));
        
        detail.bloomMask = vector<bool>(BMASK, true);
        
        for (Id b=0; b < BMASK; ++b) {
            file.get(cbit);
            if (cbit == 'o') detail.bloomMask[b] = false;
        }
        file.read((char *) &(detail.fid), sizeof(Id));
        
        _ranges.push_back(detail);
    }
    file.close();
}

set<Id> getFingerprint (const Key & key) {
    srand(key);
    set<Id> fp;
    for(int i=0; i < BBITS; ++i) {
        fp.insert( rand()%(BMASK) );
    }
    return fp;
}


bool loadFromFile (Id fid, Key key) {
    unordered_map<Key,Value> temp;
    Key loadedKey;
    bool result = false;

    char filename[512];
    snprintf(filename, 512, "%s/%" PRId32 ".bin", _swappypath, fid);
    printf("look into %s\n", filename);
    ifstream file(filename, ios::in | ios::binary);

    for (Id c = 0; c < FILE_ITEMS; ++c) {
        file.read((char *) &loadedKey, sizeof(Key));
        file.read((char *) &(temp[loadedKey]), sizeof(Value));
        if (loadedKey == key) result = true;
    }
    file.close();
    
    if (result == false) return false;

    for (auto key_val : temp) {
        _ramList[key_val.first] = key_val.second;
    }
    
    return true;
}

bool loadFromFiles (const Key & key) {
    set<Id> candidates;
    mutex m;
    set<Id> fp = getFingerprint(key);
    
    for_each (execution::par, _ranges.begin(), _ranges.end(), [&](Detail finfo) {
        if ( (key >= finfo.minimum) && (key <= finfo.maximum) ) {
            // check bloom (is it possible, that this key is in that file?)
            bool success = true;
            for (auto b : fp) {
                if (finfo.bloomMask[b] == false) {
                    success = false;
                    break;
                }
            }
            if (success) {
                lock_guard lock(m);
                candidates.insert(finfo.fid);
            }
        }
    });
    
    bool loaded = false;
    
    for (Id fid : candidates) {
        loaded = loadFromFile(fid, key);
        if (loaded) break;
    }
    return loaded;
}

Value * get(const Key & key) {
    try {
        _ramList.at(key);
        return &(_ramList[key]);

    } catch (const std::out_of_range & oor) {
        if (loadFromFiles(key)) {
            return &(_ramList[key]);
        } else {
            return nullptr;
        }
    }
}

// ------------------------------------------------------------------------
int main(int argc, char** argv) {
    snprintf(
        _swappypath, 512,
        "../temp23-%d-%d-%d-%d-%d",
        FILE_ITEMS, FILE_MULTI, RAM_MULTI, BBITS, BMASK
    );
    wakeup();
    
    start = std::chrono::high_resolution_clock::now();
    //Key query = 6913366361u;
    Key query = 309348544; //300718580; //_ranges[28].maximum;
    Value * val = get(query);
    
    if (val == nullptr) {
        printf("The '%lu' does not exist.\n", query);
    } else {
        printf("Name of '%lu': %s\n", query, val->_name);
    }
    
    auto now = std::chrono::high_resolution_clock::now();
    mseconds = std::chrono::duration<double, std::milli>(now-start).count();
    printf("end: %.f ms\n", mseconds);

    return 0;
}

