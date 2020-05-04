#ifndef __PKUH_HPP__
#define __PKUH_HPP__ 1

#include <inttypes.h>  // uintX_t stuff
#include <iostream>    // sizeof and ios namespace
#include <algorithm>   // find, minmax
#include <stdexcept>   // std::out_of_range
#include <utility>     // make_pair
#include <cstdlib>     // srand, rand, exit
#include <limits>      // min max of int... etc

#include <vector>         // store file information
#include <set>            // a set of candidate files
#include <unordered_set>  // a set of keys
#include <unordered_map>  // for a key->data
#include <deque>          // for key order (prio)
#include <map>            // for store keys in a sorted way

// we need it for PRId32 in snprintf
#define __STDC_FORMAT_MACROS

#include <fstream>                     // file operations
#include <experimental/filesystem>     // check and create directories
namespace filesys = std::experimental::filesystem;

#include <execution> // c++17 and you need  -ltbb
#include <mutex> // for mutex
#include <thread> // you need -lpthread


/**
 * this code is a simple version of SwappyItems. It is the baremetal version
 * and the first step to a external+internal priorety queue.
 * 
 * this code is never used or tested!
 */

template <int EACHFILE = 1024, int OLDIES = 4, int RAMSIZE = 3, int BLOOMBITS = 5, int MASKLENGTH = ((5+4)*1024)>
class Pkuh {

public:

    struct Element {
        uint64_t key;
        uint64_t prio;
        uint64_t successor;
    };

private:
    typedef uint32_t                                     Id;
    typedef uint32_t                                    Fid; // File id

    struct Detail {
        uint64_t minimum;
        uint64_t maximum;
        std::vector<bool> bloomMask;
        Fid fid;
    };
    
    struct Qentry {
        uint64_t key;
        bool deleted;
    };
    
    typedef uint32_t                                   Bid;
    typedef std::set<Bid>                      Fingerprint;
    
    typedef std::unordered_map<uint64_t,Element>       Ram;
    typedef std::vector<Detail>                    Buckets;
    typedef std::deque<Qentry>                       Order;

    // the item store in RAM
    Ram _ramList;
    uint64_t _ramMinimum;
    uint64_t _ramMaximum;
        
    // a priority queue for the keys
    Order _prios;

    // for each filenr we store min/max value and a bloom mask to filter
    Buckets _buckets;

    const char _prefix[7] = "./pkuh";
    char _swappypath[256];

    std::mutex mu_FsearchSuccess;
    bool fsearchSuccess;
    
public:

    bool set (uint64_t key, Element value) {
        // make it safe!
        value.key = key;
        
        if (load(key)) {
            // just update
            // reverse search should be faster
            auto it = std::find_if(std::execution::par, _prios.rbegin(), _prios.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            it->deleted = true;
            _prios.push_back(Qentry{key,false});
            _ramList[key] = value;
            return false;
        } else {
            // key is new
            maySwap();
            _prios.push_back(Qentry{key,false});
            _ramList[key] = value;
            // update min/max of RAM
            if (key > _ramMaximum) _ramMaximum = key;
            if (key < _ramMinimum) _ramMinimum = key;
            return true;
        }
    }

    Element * get (const uint64_t & key) {
        if (load(key)) {
            // reverse search should be faster
            auto it = std::find_if(std::execution::par, _prios.rbegin(), _prios.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            // every get fills the queue with "deleted = true" on this key, thus older entries 
            // are never equal false
            it->deleted = true;
            _prios.push_back(Qentry{key,false});
            return &(_ramList[key]);
        } else {
            return nullptr;
        }
    }

    uint64_t min (void) {
        uint64_t val = std::numeric_limits<TKEY>::max();
        std::for_each (_buckets.begin(), _buckets.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                if (finfo.minimum < val) val = finfo.minimum;
            }
        });
        
        if (_ramMinimum < val) val = _ramMinimum;
        return val;
    }

    uint64_t max (void) {
        uint64_t val = std::numeric_limits<TKEY>::min();
        std::for_each (_buckets.begin(), _buckets.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                if (finfo.maximum > val) val = finfo.maximum;
            }
        });
        
        if (_ramMaximum > val) val = _ramMaximum;
        return val;
    }

    Pkuh () {
        _ramMinimum = std::numeric_limits<uint64_t>::max();
        _ramMaximum = std::numeric_limits<uint64_t>::min();

        snprintf(
            _swappypath, 512,
            "%s-"
            "%d-%d-%d-%d-%d",
            _prefix,
            EACHFILE, OLDIES, RAMSIZE, BLOOMBITS, MASKLENGTH
        );
    }

private:

    Fingerprint getFingerprint (const uint64_t & key) {
        srand(key);
        Fingerprint fp;
        for(int i=0; i < BLOOMBITS; ++i) {
            fp.insert( rand()%(MASKLENGTH) );
        }
        return fp;
    }

    bool load (const uint64_t & key) {
        try {
            _ramList.at(key);
            return true;
        } catch (const std::out_of_range & oor) {
            return loadFromFiles(key);
        }
    }

    bool loadFromFiles (const uint64_t & key) {
        std::vector<Fid> candidates;
        std::mutex m;
        Fingerprint fp = getFingerprint(key);
        fsearchSuccess = false;
        
        std::for_each (std::execution::par, _buckets.begin(), _buckets.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                
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
                        m.lock();
                        candidates.push_back(finfo.fid);
                        m.unlock();
                    }
                }
            }

        });
        
        if (candidates.size() > 0) {
            // start workers
            std::vector<std::thread *> workers;
            for (unsigned i = 0; i < candidates.size(); ++i) {
                workers.push_back( new std::thread(&Pkuh::loadFromFile, this, candidates[i], key) );
            }
            // wait for workers
            for (auto w : workers) if (w->joinable()) w->join();
            // cleanup
            for (auto w : workers) delete w;
        }
        
        return fsearchSuccess;
    }

    void loadFromFile (Fid fid, uint64_t key) {
        Ram temp;
        Id length;
        uint64_t loadedKey;
        uint64_t loadedKey2;
        Element loadedValue;
        
        bool result = false;
        
        char filename[512];
        snprintf(filename, 512, "%s/%" PRId32 ".bucket", _swappypath, fid);
        std::ifstream file(filename, std::ios::in | std::ios::binary);
        
        for (Id c = 0; c < EACHFILE; ++c) {
            file.read((char *) &loadedKey, sizeof(uint64_t));
            file.read((char *) &loadedValue, sizeof(Element));
            temp[loadedKey] = loadedValue;

            if (loadedKey == key) {
                result = true;
                mu_FsearchSuccess.lock();
                fsearchSuccess = true;
                mu_FsearchSuccess.unlock();
            }
            if ((result == false) && fsearchSuccess) {
                file.close();
                return;
            }
        }
        file.close();
        
        if (result) {
            // store possible min/max for ram
            uint64_t omin = _buckets[fid].minimum;
            uint64_t omax = _buckets[fid].maximum;
            
            // ok, key exist. clean mask now, because we do not
            // need the (still undeleted) file anymore
            _buckets[fid].minimum = 0;
            _buckets[fid].maximum = 0;

            // we try to make place for the filecontent in RAM ...
            maySwap(true);
            
            // ... and load the stuff
            for (auto element : temp) {
                _ramList[element.key] = element;
                _prios.push_back(Qentry{element.key,false});
            }
            // update ram min/max
            if (omax > _ramMaximum) _ramMaximum = omax;
            if (omin < _ramMinimum) _ramMinimum = omin;
        }
    }

    void maySwap (bool reload = false) {
        Id needed = reload? EACHFILE : 0;
        Id pos;

        // no need to swap files?
        if ( (_ramList.size() + needed) < (RAMSIZE * EACHFILE * OLDIES) ) {
            // cleanup queue instead?
            if ( _prios.size() > ( (RAMSIZE+2) * EACHFILE*OLDIES) ) {
                Order newPrio;
                // remove some deleted items
                for (auto it = _prios.begin(); it != _prios.end(); ++it) {
                    if (it->deleted == false) newPrio.push_back(*it);
                }
                _prios = newPrio;
            }
            return;
        }
        
        Detail detail{0, 0};
        Fingerprint fp;
        
        std::map<uint64_t,Element> temp; // map is sorted by key!
        bool success;

        // remove old items from front and move them into temp
        for (pos = 0; pos < (EACHFILE*OLDIES); ) {
            Qentry qe = _prios.front();
            _prios.pop_front();
            if (qe.deleted == false) {
                temp[qe.key] = _ramList[qe.key];
                _ramList.erase(qe.key);
                ++pos; // we only count undeleted stuff!
            }
        }

        // run through sorted items
        Id written = 0;
        std::ofstream file;
        typename std::map<uint64_t,Element>::iterator it;

        for (it=temp.begin(); it!=temp.end(); ++it) {

            // open -----------------------------------
            if ((written%(EACHFILE)) == 0) {
                success = false;

                // find empty bucket, else create one on position ".size()"
                for (Fid fid = 0; fid < _buckets.size(); ++fid) {
                    if (_buckets[fid].minimum == _buckets[fid].maximum) {
                        pos = fid;
                        success = true;
                        break; // we found an empty Bucket
                    }
                }

                if (success == false) {
                    pos = _buckets.size();
                    // add an empty dummy Bucket
                    _buckets.push_back(detail); 
                }

                char filename[512];
                snprintf(filename, 512, "%s/%" PRId32 ".bucket", _swappypath, pos);
                file.open(filename, std::ios::out | std::ios::binary);
                detail.minimum = it->key;
                detail.bloomMask = std::vector<bool>(MASKLENGTH, false);
            }

            // store data
            file.write((char *) &(it->key), sizeof(uint64_t));
            file.write((char *) &(*it), sizeof(Element));
            
            // remember key in bloom mask
            fp = getFingerprint(it->key);
            for (auto b : fp) {
                detail.bloomMask[b] = true;
            }

            ++written;
            
            // close ----------------------------------
            if ((written%(EACHFILE)) == 0) {
                // store bucket
                detail.fid = pos;
                detail.maximum = it->key;
                _buckets[pos] = detail;
                file.close();
            }
        }
        ramMinMax();
    }
    
    void ramMinMax(void) {
        auto result = std::minmax_element(
                std::execution::par, _ramList.begin(), _ramList.end(), [](auto lhs, auto rhs
            ) {
            return (lhs.key < rhs.key); // compare the keys
        });
        //                    .------------ the min
        //                    v    v------- the key of the unordered_map entry
        _ramMinimum = result.first->key;
        //                    v------------ the max
        _ramMaximum = result.second->key;
    }

};

#endif
