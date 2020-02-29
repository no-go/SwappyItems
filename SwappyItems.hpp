#ifndef __SWAPPY_ITEMS_HPP__
#define __SWAPPY_ITEMS_HPP__ 1

#include <inttypes.h>  // uintX_t stuff
#include <iostream>    // sizeof and ios namespace
#include <algorithm>   // find
#include <stdexcept>   // std::out_of_range

#include <cstdlib>     // srand, rand, exit

#include <vector>         // store file information
#include <set>            // a set of candidate files
#include <unordered_map>  // for a key->data
#include <deque>          // for key order (prio)
#include <map>            // for store keys in a sorted way

// we need it for PRId32 in snprintf
#define __STDC_FORMAT_MACROS

#include <fstream>                     // file operations
#include <experimental/filesystem>     // check and create directories
namespace filesys = std::experimental::filesystem;

#include <execution> // c++17 and you need  -ltbb 

#include <thread> // you need -lpthread
#include <atomic>
#include "Semaphore.hpp"

/**
 * a 0 as Key means, that this key/value pair is deleted, thus
 * never use 0 as a key! this data may be ignored sometimes.
 */
template <
    class TKEY, class TVALUE,
    int EACHFILE = 16384, int OLDIES = 5, int RAMSIZE = 3, int BLOOMBITS = 4, int MASKLENGTH = (2* 4*16384)
>
class SwappyItems {

    typedef uint32_t                               Id;
    typedef uint32_t                              Fid; // File id

    struct Detail {
        TKEY minimum;
        TKEY maximum;
        std::vector<bool> bloomMask;
        Fid fid;
    };
    
    struct Qentry {
        TKEY key;
        bool deleted;
    };

    typedef uint32_t                              Bid;
    typedef std::set<Bid>                 Fingerprint;

    typedef std::unordered_map<TKEY,TVALUE>       Ram;
    typedef std::vector<Detail>                Ranges;
    typedef std::deque<Qentry>                  Order;

    // the item store in RAM
    Ram _ramList;

    // a priority queue for the keys
    Order _prios;

    // for each filenr we store min/max value and a bloom mask to filter
    Ranges _ranges;

    int _swappyId = 0;
    const char _prefix[7] = "./temp"; // limit 120 chars (incl. 2 numbers and ".bin")
    
    Semaphore * _s;
    int _cores;
    std::atomic<bool> _loaded;
    
public:

    struct statistic_s {
        uint64_t counting = 0;
        uint64_t updates = 0;

        uint64_t bloomSaysFresh = 0;
        uint64_t bloomFails = 0;

        uint64_t rangeSaysNo = 0;
        uint64_t rangeFails = 0;

        uint64_t swaps = 0;
        uint64_t fileLoads = 0;
    } statistic;

    /**
     * set (create or update)
     *
     * @return true if it is new
     */
    bool set (TKEY key, TVALUE value) {
        if (load(key)) {
            // just update
            /// @todo reverse search should be faster
            auto it = std::find_if(std::execution::par, _prios.begin(), _prios.end(), 
                [key] (const Qentry & qe) {
                    return qe.key == key;
                }
            );
            it->deleted = true;
            _prios.push_back(Qentry{key,false});
            _ramList[key] = value;
            ++statistic.updates;
            return false;
        } else {
            // key is new
            maySwap();
            ++statistic.counting;
            _prios.push_back(Qentry{key,false});
            _ramList[key] = value;
            return true;
        }
    }

    /**
     * get a item
     *
     * @param key the unique key
     * @return nullptr if item not exists
     */
    TVALUE * get (const TKEY & key) {
        if (load(key)) {
            /// @todo reverse search should be faster
            auto it = std::find_if(std::execution::par, _prios.begin(), _prios.end(), 
                [key] (const Qentry & qe) {
                    return qe.key == key;
                }
            );
            it->deleted = true;
            _prios.push_back(Qentry{key,false});
            return &(_ramList[key]);
        } else {
            return nullptr;
        }
    }
    
    /**
     * delete a item
     *
     * @param key the unique key
     * @return false if item not exists
     */
    bool del (const TKEY & key) {
        if (load(key)) {
            /// @todo reverse search should be faster
            auto it = std::find_if(std::execution::par, _prios.begin(), _prios.end(), 
                [key] (const Qentry & qe) {
                    return qe.key == key;
                }
            );
            it->deleted = true;
            --statistic.counting;
            return true;
        } else {
            return false;
        }
    }
    
    /**
     * stores all RAM content into file(s)
     */
    void hibernate () {
        char filename[120];

        snprintf(filename, 120, "%s%d/hibernate.ramlist", _prefix, _swappyId);
        std::ofstream file(filename, std::ios::out | std::ios::binary);
        for (auto it = _ramList.begin(); it != _ramList.end(); ++it) {
            file.write((char *) &(it->first), sizeof(TKEY));
            file.write((char *) &(it->second), sizeof(TVALUE));
        }
        file.close();

        snprintf(filename, 120, "%s%d/hibernate.statistic", _prefix, _swappyId);
        file.open(filename, std::ios::out | std::ios::binary);
        file.write((char *) &statistic, sizeof(statistic_s));
        file.close();

        snprintf(filename, 120, "%s%d/hibernate.prios", _prefix, _swappyId);
        file.open(filename, std::ios::out | std::ios::binary);
        for (auto it = _prios.begin(); it != _prios.end(); ++it) {
            file.write((char *) &(it->key), sizeof(Qentry::key));
            file.write((char *) &(it->deleted), sizeof(Qentry::deleted));
        }
        file.close();

        snprintf(filename, 120, "%s%d/hibernate.ranges", _prefix, _swappyId);
        file.open(filename, std::ios::out | std::ios::binary);
        for (auto it = _ranges.begin(); it != _ranges.end(); ++it) {
            file.write((char *) &(it->minimum), sizeof(Detail::minimum));
            file.write((char *) &(it->maximum), sizeof(Detail::maximum));
            for (bool b : it->bloomMask) {
                if (b) {
                    file.put('i');
                } else {
                    file.put('o');
                }
            }
            file.write((char *) &(it->fid), sizeof(Detail::fid));
        }
        file.close();
    }

    /**
     * with parameter true you get the number of items in ram
     * 
     * @todo add both to statistics ?
     */
    uint64_t size(bool ramOnly = false) {
        if (ramOnly) return _ramList.size();
        return statistic.counting;
    }

    /**
     * get the size of the priority queue, which may be bigger than
     * the items in ram (items marked as deleted)
     * 
     * @todo add to statistics ?
     */
    uint64_t prioSize() {
        return _prios.size();
    }

    SwappyItems (int swappyId) {
        statistic.counting = 0;
        statistic.updates = 0;

        statistic.bloomSaysFresh = 0;
        statistic.rangeSaysNo = 0;
        statistic.rangeFails = 0;

        statistic.swaps = 0;
        statistic.fileLoads = 0;
        
        _swappyId = swappyId;
        char filename[120];
        snprintf(filename, 120, "%s%d", _prefix, _swappyId);
        if (filesys::exists(filename)) {
            
            
            
            /// @todo try to wake up after hibernate
            
            
            
            fprintf(stderr, "folder %s still exists! Please delete or rename it!\n", filename);
            exit(0);
        } else {
            filesys::create_directory(filename);
        }

        _cores = std::thread::hardware_concurrency();
        _s = new Semaphore(_cores);
    }
    
    ~SwappyItems () {
        delete _s;
    }


private:

    Fingerprint getFingerprint (const TKEY & key) {
        srand(key);
        Fingerprint fp;
        for(int i=0; i < BLOOMBITS; ++i) {
            fp.insert( rand()%(MASKLENGTH) );
        }
        return fp;
    }

    /**
     * try to load a key/value into ramList
     *
     * @return false, if key not exists
     */
    bool load (const TKEY & key) {
        try {
            _ramList.at(key);
            return true;
        } catch (const std::out_of_range & oor) {
            return loadFromFiles(key);
        }
    }

    /**
     * search a key in all (maybe) relevant files and load it to ram
     *
     * @return false, if not exists
     */
    bool loadFromFiles (const TKEY & key) {
        std::set<Fid> candidates;
        std::mutex m;
        Fingerprint fp = getFingerprint(key);
        
        //for(Fid fid = 0; fid < _ranges.size(); ++fid) {
        std::for_each (std::execution::par, _ranges.begin(), _ranges.end(), [&](Detail finfo) {
            if ( (key < finfo.minimum) || (key > finfo.maximum) ) {
                // key is smaller then the smallest or bigger than the biggest
                std::lock_guard lock(m);
                ++statistic.rangeSaysNo;
            } else {
                // check bloom (is it possible, that this key is in that file?)
                bool success = true;
                for (auto b : fp) {
                    if (finfo.bloomMask[b] == false) {
                        success = false;
                        std::lock_guard lock(m);
                        ++statistic.bloomSaysFresh;
                        break;
                    }
                }
                if (success) {
                    std::lock_guard lock(m);
                    candidates.insert(finfo.fid);
                }
            }
        });
        
        _loaded = false;
        
        for (Fid fid : candidates) {
            std::thread t(&SwappyItems::loadFromFile, this, fid, key);
            t.detach();
            _s->P();
            // the key was loaded into ram, because the file has it
            if (_loaded) break;
        }
        
        // wait until all threads are finish
        while(_s->_count < _cores);

        if (_loaded == false) {
            if (candidates.size() > 0) {
                // many candidates, but finaly not found in file
                ++statistic.rangeFails;
            }
            return false;
        } else {
            ++statistic.fileLoads;
            return true;
        }
    }

    /**
     * search a key in a file: LOAD
     *
     * if key exists, then load the file into _ramlist (did not delete the file, but
     * file may be overwritten), may save old data in a new file (swap).
     *
     * @param filenr the index of the file and bitmask id
     * @param key the key we are searching for
     * @return true, if key is loaded into ram
     */
    void loadFromFile (Fid fid, TKEY key) {
        // @todo Items are sorted in file! binary search?

        Ram temp;
        TKEY loadedKey;
        TVALUE loadedVal;
        bool result = false;

        char filename[120];
        snprintf(filename, 120, "%s%d/%" PRId32 ".bin", _prefix, _swappyId, fid);
        std::ifstream file(filename, std::ios::in | std::ios::binary);

        for (Id c = 0; c < EACHFILE; ++c) {
            file.read((char *) &loadedKey, sizeof(TKEY));
            file.read((char *) &loadedVal, sizeof(TVALUE));
            if (loadedKey == key) {
                result = true;
            } else {
                // stop this search, because a other thread find the key
                if (_loaded) break;
            }
            temp[loadedKey] = loadedVal;
        }
        file.close();

        if (result == false) {
             // key not exists
            _s->V();
            return;
        }

        // ok, key exist. clean mask now, because we do not
        // need the (still undeleted) file anymore
        _ranges[fid] = Detail{0, 0};

        // we try to make place for the filecontent in RAM ...
        maySwap(true);
        // ... and load the stuff
        for (auto key_val : temp) {
            _ramList[key_val.first] = key_val.second;
            _prios.push_back(Qentry{key_val.first,false});
        }
        
        _loaded = true;
        _s->V();
        return;
    }

    /**
     * swap to file: SAVE
     *
     * if ramlist is full, then OLDIES*EACHFILE Items are written to files
     *
     * @param reload if it is set, then it swaps to _
     *        file if (ramlist.size + EACHFILE) >= allowed number of items in ramlist
     */
    void maySwap (bool reload = false) {
        Id needed = reload? EACHFILE : 0;

        // no need to swap files?
        if ( (_ramList.size() + needed) < (RAMSIZE * EACHFILE * OLDIES) ) return;

        Id pos;
        Detail detail{0, 0};
        Fingerprint fp;
        
        std::map<TKEY,TVALUE> temp; // map is sorted by key!
        bool success;
        
        ++statistic.swaps;

        // remove old items from front and move them to temp
        for (pos = 0; pos < (OLDIES*EACHFILE); ) {
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
        typename std::map<TKEY,TVALUE>::iterator it;

        for (it=temp.begin(); it!=temp.end(); ++it) {

            if ((written%(EACHFILE)) == 0) {
                success = false;

                // find empty range, else create one on position ".size()"
                for (Fid fid = 0; fid < _ranges.size(); ++fid) {
                    if (_ranges[fid].minimum == _ranges[fid].maximum) {
                        pos = fid;
                        success = true;
                        break; // we found an empty Range
                    }
                }

                if (success == false) {
                    pos = _ranges.size();
                    // add an empty dummy Range
                    _ranges.push_back( Detail{0, 0} ); 
                }

                char filename[120];
                snprintf(filename, 120, "%s%d/%" PRId32 ".bin", _prefix, _swappyId, pos);
                file.open(filename, std::ios::out | std::ios::binary);
                detail.minimum = it->first;
                detail.bloomMask = std::vector<bool>(MASKLENGTH, false);
            }

            // store data
            file.write((char *) &(it->first), sizeof(TKEY));
            file.write((char *) &(it->second), sizeof(TVALUE));

            fp = getFingerprint(it->first);
            for (auto b : fp) {
                detail.bloomMask[b] = true;
            }
            ++written;

            if ((written%(EACHFILE)) == 0) {
                // store range
                detail.fid = pos;
                detail.maximum = it->first;
                _ranges[pos] = detail;
                file.close();
            }
        }
    }
};

#endif
