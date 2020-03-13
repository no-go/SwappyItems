#ifndef __SWAPPY_ITEMS_HPP__
#define __SWAPPY_ITEMS_HPP__ 1

#include <inttypes.h>  // uintX_t stuff
#include <iostream>    // sizeof and ios namespace
#include <algorithm>   // find
#include <stdexcept>   // std::out_of_range
#include <utility>     // make_pair
#include <cstdlib>     // srand, rand, exit

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
#include <mutex> // for std::lock_guard and mutex
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

public:
    typedef std::pair<TVALUE, std::vector<TKEY> >     Data; // just for simplifing next line
    
    struct statistic_s {
        uint64_t size    = 0;
        uint64_t fileKB  = 0;
        uint64_t queue   = 0;
        
        uint64_t updates = 0;
        uint64_t deletes = 0;

        uint64_t bloomSaysNotIn = 0;
        uint64_t bloomFails = 0;

        uint64_t rangeSaysNo = 0;
        uint64_t rangeFails = 0;

        uint64_t swaps = 0;
        uint64_t fileLoads = 0;
    };
    
private:
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
    
    typedef uint32_t                                   Bid;
    typedef std::set<Bid>                      Fingerprint;
    
    typedef std::unordered_map<TKEY,Data>              Ram;
    typedef std::vector<Detail>                     Ranges;
    typedef std::deque<Qentry>                       Order;

    // the item store in RAM
    Ram _ramList;

    // a priority queue for the keys
    Order _prios;

    // for each filenr we store min/max value and a bloom mask to filter
    Ranges _ranges;

    const char _prefix[7] = "./temp";
    char _swappypath[256];

    std::atomic<bool> keyFound;
    Semaphore * S;
    std::atomic<unsigned> done;

    struct statistic_s statistic;
    
public:

    /**
     * set (create or update)
     *
     * @return true if it is new
     */
    bool set (TKEY key, TVALUE value) {
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
            _ramList[key] = std::make_pair(value, _ramList[key].second);
            ++statistic.updates;
            return false;
        } else {
            // key is new
            maySwap();
            ++statistic.size;
            _prios.push_back(Qentry{key,false});
            _ramList[key] = std::make_pair(value, std::vector<TKEY>(0));
            return true;
        }
    }

    /**
     * set (create or update)
     *
     * @return true if it is new
     */
    bool set (TKEY key, TVALUE value, std::vector<TKEY> refs) {
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
            _ramList[key] = std::make_pair(value, refs);
            ++statistic.updates;
            return false;
        } else {
            // key is new
            maySwap();
            ++statistic.size;
            _prios.push_back(Qentry{key,false});
            _ramList[key] = std::make_pair(value, refs);
            return true;
        }
    }

    /**
     * get a item
     *
     * @param key the unique key
     * @return nullptr if item not exists
     */
    Data * get (const TKEY & key) {
        if (load(key)) {
            // reverse search should be faster
            auto it = std::find_if(std::execution::par, _prios.rbegin(), _prios.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
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
            // reverse search should be faster
            auto it = std::find_if(std::execution::par, _prios.rbegin(), _prios.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            it->deleted = true;
            --statistic.size;
            ++statistic.deletes;
            return true;
        } else {
            return false;
        }
    }
    
    /**
     * This "for_each" catches all items and runs a function on each item.
     * If this function returns true, the "for_each" stops.
     * 
     * @todo foreach multithread
     * @todo load next range in background
     * 
     * @param back is pointer to the value, were the each loop breaks
     * @param foo is a lambda function, which gets each key and value(pair). if foo return true, the each-loop breaks
     * @return if it is false, the loop did not break
     */
    bool each (Data & back, std::function<bool(TKEY, Data &)> foo) {
        typename Ram::iterator it;
        std::set<Fid> important;
                
        // run in RAM
        /// @todo foreach multithread
        for (it = _ramList.begin(); it != _ramList.end(); ++it) {
            if (foo(it->first, it->second)) {
                back = it->second;
                return true;
            }
        }
        // store all undeleted ranges, because they have the other items
        std::for_each (_ranges.begin(), _ranges.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                important.insert(finfo.fid);
            }
        });
        
        
        for (Fid fid : important) {
            // 1) load into temp each range/file to get all keys of the file
            Ram temp;
            std::unordered_set<TKEY> keys;
            Id length;
            TKEY loadedKey;
            TKEY loadedKey2;
            TVALUE loadedValue;

            char filename[512];
            snprintf(filename, 512, "%s/%" PRId32 ".bin", _swappypath, fid);
            std::ifstream file(filename, std::ios::in | std::ios::binary);

            for (Id c = 0; c < EACHFILE; ++c) {
                std::vector<TKEY> vecdata(0);
                file.read((char *) &loadedKey, sizeof(TKEY));
                file.read((char *) &loadedValue, sizeof(TVALUE));
                keys.insert(loadedKey);
                // stored vector?
                file.read((char *) &length, sizeof(Id));
                if (length > 0) {
                    for (Id j=0; j<length; ++j) {
                        file.read((char *) &loadedKey2, sizeof(TKEY));
                        vecdata.push_back(loadedKey2);
                    }
                }
                temp[loadedKey] = std::make_pair(loadedValue, vecdata);
            }
            file.close();
            // 2) mark file as empty --------------------- 
            _ranges[fid].minimum = 0;
            _ranges[fid].maximum = 0;
            // 3) may swap and place temp into ram ---------- 
            maySwap(true);
            // ... and load the stuff
            for (auto key_val : temp) {
                _ramList[key_val.first] = key_val.second;
                _prios.push_back(Qentry{key_val.first,false});
            }
            // 4) run through in RAM with keys from temp
            for (auto k : keys) {
                if (foo(k, _ramList[k])) {
                    back = _ramList[k];
                    return true;
                }
            }
        }
        
        return false;
    }
    
    struct statistic_s getStatistic() {
        statistic.fileKB = (_ranges.size() * EACHFILE * (sizeof(TKEY) + sizeof(TVALUE))/1000);
        statistic.queue = _prios.size();
        return statistic;
    }

    SwappyItems (int swappyId) {
        S = new Semaphore(std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency()-1 : 1);
        
        statistic.size   = 0;
        statistic.fileKB = 0;
        statistic.queue  = 0;
        
        statistic.updates = 0;
        statistic.deletes = 0;

        statistic.bloomSaysNotIn = 0;
        statistic.rangeSaysNo = 0;
        statistic.rangeFails = 0;

        statistic.swaps = 0;
        statistic.fileLoads = 0;
        
        // make quasi hash from (swappyId, template parameter) instead of just swappyId.
        // this should fix problems with other template paramter and loading hibernate data!
        snprintf(
            _swappypath, 512,
            "%s%d-"
            "%d-%d-%d-%d-%d",
            _prefix,
            swappyId,
            EACHFILE, OLDIES, RAMSIZE, BLOOMBITS, MASKLENGTH
        );
        
        if (filesys::exists(_swappypath)) {
            // try to wake up after hibernate
            if (false == wakeup()) {
                fprintf(stderr, "folder '%s' still exists without hibernate data! Please delete or rename it!\n", _swappypath);
                exit(0);
            } else {
                printf(
                    "# wakeup on items, updates, deletes: "
                    "%10ld %10" PRId64 " %10" PRId64 "\n",
                    statistic.size,
                    statistic.updates,
                    statistic.deletes
                );
            }
        } else {
            filesys::create_directory(_swappypath);
        }
    }
    
    ~SwappyItems () {
        hibernate();
        delete S;
    }

private:

    /**
     * load all hibernate content into RAM
     * 
     * @todo return on fail
     */
    bool wakeup () {
        char filename[512];
        TKEY loadedKey;
        TKEY loadedKey2;
        TVALUE loadedValue;
        std::ifstream file;

        snprintf(filename, 512, "%s/hibernate.ramlist", _swappypath);
        file.open(filename, std::ios::in | std::ios::binary);
        Id length;
        Id lengthVec;
        file.read((char *) &length, sizeof(Id));
        for (Id c = 0; c < length; ++c) {
            std::vector<TKEY> vecdata(0);
            file.read((char *) &loadedKey, sizeof(TKEY));
            file.read((char *) &loadedValue, sizeof(TVALUE));
            // stored vector?
            file.read((char *) &lengthVec, sizeof(Id));
            for (Id j=0; j<lengthVec; ++j) {
                file.read((char *) &loadedKey2, sizeof(TKEY));
                vecdata.push_back(loadedKey2);
            }
            _ramList[loadedKey] = std::make_pair(loadedValue, vecdata);
        }
        file.close();

        snprintf(filename, 512, "%s/hibernate.statistic", _swappypath);
        file.open(filename, std::ios::in | std::ios::binary);
        file.read((char *) &statistic, sizeof(statistic_s));
        file.close();

        snprintf(filename, 512, "%s/hibernate.prios", _swappypath);
        file.open(filename, std::ios::in | std::ios::binary);
        file.read((char *) &length, sizeof(Id));
        Qentry qe;
        for (Id c = 0; c < length; ++c) {
            file.read((char *) &(qe.key), sizeof(TKEY));
            file.read((char *) &(qe.deleted), sizeof(Qentry::deleted));
            _prios.push_back(qe);
        }
        file.close();
        
        snprintf(filename, 512, "%s/hibernate.ranges", _swappypath);
        file.open(filename, std::ios::in | std::ios::binary);
        file.read((char *) &length, sizeof(Id));
        Detail detail;
        char cbit;
        for (Fid c = 0; c < length; ++c) {
            file.read((char *) &(detail.minimum), sizeof(TKEY));
            file.read((char *) &(detail.maximum), sizeof(TKEY));
            
            detail.bloomMask = std::vector<bool>(MASKLENGTH, true);
            
            for (Bid b=0; b < MASKLENGTH; ++b) {
                file.get(cbit);
                if (cbit == 'o') detail.bloomMask[b] = false;
            }
            file.read((char *) &(detail.fid), sizeof(Fid));
            
            _ranges.push_back(detail);
        }
        file.close();
        
        return true;
    }

    /**
     * stores all RAM content into file(s)
     * 
     * strg+c -> make a regular delete to do a hibernate!
     */
    void hibernate () {
        char filename[512];

        snprintf(filename, 512, "%s/hibernate.ramlist", _swappypath);
        std::ofstream file(filename, std::ios::out | std::ios::binary);
        Id length = _ramList.size();
        file.write((char *) &length, sizeof(Id));
        for (auto it = _ramList.begin(); it != _ramList.end(); ++it) {
            file.write((char *) &(it->first), sizeof(TKEY));
            file.write((char *) &(it->second.first), sizeof(TVALUE));
            // stored vector?
            length = it->second.second.size();
            file.write((char *) &length, sizeof(Id));
            for (Id j=0; j<length; ++j) {
                file.write((char *) &(it->second.second[j]), sizeof(TKEY));
            }
        }
        file.close();

        snprintf(filename, 512, "%s/hibernate.statistic", _swappypath);
        file.open(filename, std::ios::out | std::ios::binary);
        file.write((char *) &statistic, sizeof(statistic_s));
        file.close();

        snprintf(filename, 512, "%s/hibernate.prios", _swappypath);
        file.open(filename, std::ios::out | std::ios::binary);
        length = _prios.size();
        file.write((char *) &length, sizeof(Id));
        for (auto it = _prios.begin(); it != _prios.end(); ++it) {
            file.write((char *) &(it->key), sizeof(TKEY));
            file.write((char *) &(it->deleted), sizeof(Qentry::deleted));
        }
        file.close();

        snprintf(filename, 512, "%s/hibernate.ranges", _swappypath);
        file.open(filename, std::ios::out | std::ios::binary);
        length = _ranges.size();
        file.write((char *) &length, sizeof(Id));
        for (auto it = _ranges.begin(); it != _ranges.end(); ++it) {
            file.write((char *) &(it->minimum), sizeof(TKEY));
            file.write((char *) &(it->maximum), sizeof(TKEY));
            for (bool b : it->bloomMask) {
                if (b) {
                    file.put('i');
                } else {
                    file.put('o');
                }
            }
            file.write((char *) &(it->fid), sizeof(Fid));
        }
        file.close();
    }

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
        
        std::for_each (std::execution::par, _ranges.begin(), _ranges.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                
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
                            ++statistic.bloomSaysNotIn;
                            break;
                        }
                    }
                    if (success) {
                        std::lock_guard lock(m);
                        candidates.insert(finfo.fid);
                    }
                }
            }
        });
        
        keyFound = false;
        done = 0;

        for (auto fid : candidates) {
            if (!keyFound) {
                std::thread th(&SwappyItems::loadFromFile, this, fid, key);
                th.detach();
                S->P();
            }
        }
        if (candidates.size() > 0) while (done < candidates.size());

        if (keyFound) {
            ++statistic.fileLoads;
            return true;
        } else {
            if (candidates.size() > 0) {
                // many candidates, but finaly not found in file
                ++statistic.rangeFails;
            }
            return false;
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
     * @ return true, if key is loaded into ram
     */
    void loadFromFile (Fid fid, TKEY key) {
        Ram temp;
        Id length;
        TKEY loadedKey;
        TKEY loadedKey2;
        TVALUE loadedValue;
        
        bool result = false;

        char filename[512];
        snprintf(filename, 512, "%s/%" PRId32 ".bin", _swappypath, fid);
        if (keyFound) {
            done++;
            S->V();
            return;
        }
        std::ifstream file(filename, std::ios::in | std::ios::binary);

        for (Id c = 0; c < EACHFILE; ++c) {
            std::vector<TKEY> vecdata(0);
            file.read((char *) &loadedKey, sizeof(TKEY));
            file.read((char *) &loadedValue, sizeof(TVALUE));
            // stored vector?
            file.read((char *) &length, sizeof(Id));
            if (length > 0) {
                for (Id j=0; j<length; ++j) {
                    file.read((char *) &loadedKey2, sizeof(TKEY));
                    vecdata.push_back(loadedKey2);
                }
            }
            temp[loadedKey] = std::make_pair(loadedValue, vecdata);

            if (loadedKey == key) {
                result = true;
                keyFound = true;
            }
            if ((result == false) && keyFound) {
                file.close();
                done++;
                S->V();
                return;
            }
        }
        file.close();
        
        if (result) {
            // ok, key exist. clean mask now, because we do not
            // need the (still undeleted) file anymore
            _ranges[fid].minimum = 0;
            _ranges[fid].maximum = 0;

            // we try to make place for the filecontent in RAM ...
            maySwap(true);
            // ... and load the stuff
            for (auto key_val : temp) {
                _ramList[key_val.first] = key_val.second;
                _prios.push_back(Qentry{key_val.first,false});
            }
        }
        done++;
        S->V();
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
        
        std::map<TKEY,Data> temp; // map is sorted by key!
        bool success;
        
        ++statistic.swaps;

        // remove old items from front and move them to temp
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
        typename std::map<TKEY,Data>::iterator it;

        for (it=temp.begin(); it!=temp.end(); ++it) {

            // open -----------------------------------
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
                    _ranges.push_back(detail); 
                }

                char filename[512];
                snprintf(filename, 512, "%s/%" PRId32 ".bin", _swappypath, pos);
                file.open(filename, std::ios::out | std::ios::binary);
                detail.minimum = it->first;
                detail.bloomMask = std::vector<bool>(MASKLENGTH, false);
            }

            // store data
            file.write((char *) &(it->first), sizeof(TKEY));
            file.write((char *) &(it->second.first), sizeof(TVALUE));
            
            // store vector?
            Id length = it->second.second.size();
            file.write((char *) &length, sizeof(Id));
            for (Id j=0; j<length; ++j) {
                file.write((char *) &(it->second.second[j]), sizeof(TKEY));
            }
            
            // remember key in bloom mask
            fp = getFingerprint(it->first);
            for (auto b : fp) {
                detail.bloomMask[b] = true;
            }

            ++written;
            
            // close ----------------------------------
            if ((written%(EACHFILE)) == 0) {
                // store range
                detail.fid = pos;
                detail.maximum = it->first;
                _ranges[pos] = detail;
                file.close();
            }
        }
    }

// todo ---------------------------------------------------------------------------------------------------

public:

    /**
     * get a item (for use in a "each-loop" in the same SwappyItems instance)
     * 
     * this method never touches the priority queue and never swaps or load data into RAM
     *
     * @param result the value as copy
     * @param key the unique key
     * 
     * @return true, if exists
     */
    bool flat_get (Data & result, const TKEY & key) {
        
        try {
            result = _ramList.at(key);
            return true;
        } catch (const std::out_of_range & oor) {
            if (flat_loadFromFiles(result, key)) {
                return true;
            } else {
                return false;
            }
        }
    }

private:

    bool flat_loadFromFiles (Data & result, const TKEY & key) {
        std::set<Fid> candidates;
        std::mutex m;
        Fingerprint fp = getFingerprint(key);
        
        std::for_each (std::execution::par, _ranges.begin(), _ranges.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                
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
                            ++statistic.bloomSaysNotIn;
                            break;
                        }
                    }
                    if (success) {
                        std::lock_guard lock(m);
                        candidates.insert(finfo.fid);
                    }
                }
            }
        });
        
        bool success = false;

        for (auto fid : candidates) {

            Id length;
            TKEY loadedKey;
            TKEY loadedKey2;
            TVALUE loadedValue;
            
            char filename[512];
            snprintf(filename, 512, "%s/%" PRId32 ".bin", _swappypath, fid);
            std::ifstream file(filename, std::ios::in | std::ios::binary);

            for (Id c = 0; c < EACHFILE; ++c) {
                file.read((char *) &loadedKey, sizeof(TKEY));

                if (loadedKey == key) {
                    success = true;
                    
                    std::vector<TKEY> vecdata(0);
                    file.read((char *) &loadedValue, sizeof(TVALUE));
                    // stored vector?
                    file.read((char *) &length, sizeof(Id));
                    if (length > 0) {
                        for (Id j=0; j<length; ++j) {
                            file.read((char *) &loadedKey2, sizeof(TKEY));
                            vecdata.push_back(loadedKey2);
                        }
                    }
                    result = std::make_pair(loadedValue, vecdata);
                    break;

                } else {
                    /// @todo a better way to set the filepointer to the next key-value pair!
                    file.read((char *) &loadedValue, sizeof(TVALUE));
                    // stored vector?
                    file.read((char *) &length, sizeof(Id));
                    for (Id j=0; j<length; ++j) file.read((char *) &loadedKey2, sizeof(TKEY));
                }
            }
            file.close();
            if (success) break;
        }

        if (success) {
            ++statistic.fileLoads;
            return true;
        } else {
            if (candidates.size() > 0) {
                // many candidates, but finaly not found in file
                ++statistic.rangeFails;
            }
            return false;
        }
    }


};

#endif
