#ifndef __SWAPPY_ITEMS_HPP__
#define __SWAPPY_ITEMS_HPP__ 1

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
#include <deque>          // for key order (mru)
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
 * @todo still "TKEY = uint64_t" is expected ?
 */
template <
    class TKEY, class TVALUE,
    int EACHFILE = 16384, int OLDIES = 5, int RAMSIZE = 3, int BLOOMBITS = 4, int MASKLENGTH = (2* 4*16384)
>
class SwappyItems {

public:
    typedef std::pair<TVALUE, std::vector<TKEY> >  Data; // just for simplifing next line
    
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
        
        TKEY maxKey;
        TKEY minKey;
    };
    
private:
    typedef uint32_t                                 Id;
    typedef uint32_t                                Fid; // File id

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
    typedef std::vector<Detail>                    Buckets;
    typedef std::deque<Qentry>                       Order;

    // the item store in RAM
    Ram _ramList;
    TKEY _ramMinimum;
    TKEY _ramMaximum;
        
    // a queue for the keys
    Order _mru;

    // for each filenr we store min/max value and a bloom mask to filter
    Buckets _buckets;
    struct statistic_s statistic;

    const char _prefix[7] = "./temp";
    char _swappypath[256];

    std::mutex mu_FsearchSuccess;
    bool fsearchSuccess;
    
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
            auto it = std::find_if(std::execution::par, _mru.rbegin(), _mru.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            it->deleted = true;
            _mru.push_back(Qentry{key,false});
            _ramList[key] = std::make_pair(value, _ramList[key].second);
            ++statistic.updates;
            return false;
        } else {
            // key is new
            maySwap();
            ++statistic.size;
            _mru.push_back(Qentry{key,false});
            _ramList[key] = std::make_pair(value, std::vector<TKEY>(0));
            // update min/max of RAM
            if (key > _ramMaximum) _ramMaximum = key;
            if (key < _ramMinimum) _ramMinimum = key;
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
            auto it = std::find_if(std::execution::par, _mru.rbegin(), _mru.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            it->deleted = true;
            _mru.push_back(Qentry{key,false});
            _ramList[key] = std::make_pair(value, refs);
            ++statistic.updates;
            return false;
        } else {
            // key is new
            maySwap();
            ++statistic.size;
            _mru.push_back(Qentry{key,false});
            _ramList[key] = std::make_pair(value, refs);
            // update min/max of RAM
            if (key > _ramMaximum) _ramMaximum = key;
            if (key < _ramMinimum) _ramMinimum = key;
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
            auto it = std::find_if(std::execution::par, _mru.rbegin(), _mru.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            // every get fills the queue with "deleted = true" on this key, thus older entries are never equal false
            it->deleted = true;
            _mru.push_back(Qentry{key,false});
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
            auto it = std::find_if(std::execution::par, _mru.rbegin(), _mru.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            it->deleted = true;
            // we really have to delete it in ram, because load() search key initialy in ram!
            _ramList.erase(key);
            
            --statistic.size;
            ++statistic.deletes;
            
            if (key == _ramMaximum || key == _ramMinimum) ramMinMax();
            return true;
        } else {
            return false;
        }
    }
    
    /*
    TKEY min(void) {
        TKEY omin = std::numeric_limits<TKEY>::max();
        /// @todo makes multithread sense here? maybe collect mins from threads and get min of them?
        auto result = std::min(std::execution::par, _buckets, [](Detail const & lhs, Detail const & rhs) {
            /// @todo does this realy ignore deleted values?
            
            // a deleted left value should never be a minimum
            if (lhs.minimum == lhs.maximum) return false;
            // a deleted right value should never be bigger
            if (rhs.minimum == rhs.maximum) return true;
            
            return (lhs.minimum < rhs.minimum);
        });
        omin = result.minimum;
        _ramMaximum = result.second;
        return _ramMinimum;
    }
    */

    /**
     * return the smallest key
     */
    TKEY min (void) {
        TKEY val = std::numeric_limits<TKEY>::max();
        /// @todo makes multithread sense here? maybe collect mins from threads and get min of them?
        std::for_each (_buckets.begin(), _buckets.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                if (finfo.minimum < val) val = finfo.minimum;
            }
        });
        
        if (_ramMinimum < val) val = _ramMinimum;
        return val;
    }

    /**
     * return the biggest key
     */
    TKEY max (void) {
        TKEY val = std::numeric_limits<TKEY>::min();
        /// @todo makes multithread sense here? maybe collect mins from threads and get min of them?
        std::for_each (_buckets.begin(), _buckets.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                if (finfo.maximum > val) val = finfo.maximum;
            }
        });
        
        if (_ramMaximum > val) val = _ramMaximum;
        return val;
    }
    
    /**
     * get a struct with many statistic values
     */
    struct statistic_s getStatistic (void) {
        statistic.fileKB = (_buckets.size() * EACHFILE * (sizeof(TKEY) + sizeof(TVALUE))/1000);
        statistic.queue = _mru.size();
        statistic.maxKey = this->max();
        statistic.minKey = this->min();
        return statistic;
    }

    /**
     * It trys to wakeup from hibernate files. The id parameter identifies the 
     * SwappyItems Store (important for the file path!).
     * 
     * @param swappyId parameter identifies the instance
     */
    SwappyItems (int swappyId) {
        
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
        
        _ramMinimum = std::numeric_limits<TKEY>::max();
        _ramMaximum = std::numeric_limits<TKEY>::min();
        
        statistic.minKey = _ramMinimum;
        statistic.maxKey = _ramMaximum;
        
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
    
    /**
     * Makes a hibernate from ram datas to files.
     */
    ~SwappyItems (void) {
        hibernate();
    }

private:

    /**
     * load all hibernate content into RAM
     * 
     * @todo return on fail
     */
    bool wakeup (void) {
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
        file.read((char *) &_ramMinimum, sizeof(TKEY));
        file.read((char *) &_ramMaximum, sizeof(TKEY));
        file.close();

        snprintf(filename, 512, "%s/hibernate.mru", _swappypath);
        file.open(filename, std::ios::in | std::ios::binary);
        file.read((char *) &length, sizeof(Id));
        Qentry qe;
        for (Id c = 0; c < length; ++c) {
            file.read((char *) &(qe.key), sizeof(TKEY));
            file.read((char *) &(qe.deleted), sizeof(Qentry::deleted));
            _mru.push_back(qe);
        }
        file.close();
        
        snprintf(filename, 512, "%s/hibernate.buckets", _swappypath);
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
            
            _buckets.push_back(detail);
        }
        file.close();
        
        return true;
    }

    /**
     * stores all RAM content into file(s)
     * 
     * strg+c -> make a regular delete to do a hibernate!
     */
    void hibernate (void) {
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
        file.write((char *) &_ramMinimum, sizeof(TKEY));
        file.write((char *) &_ramMaximum, sizeof(TKEY));
        file.close();

        snprintf(filename, 512, "%s/hibernate.mru", _swappypath);
        file.open(filename, std::ios::out | std::ios::binary);
        length = _mru.size();
        file.write((char *) &length, sizeof(Id));
        for (auto it = _mru.begin(); it != _mru.end(); ++it) {
            file.write((char *) &(it->key), sizeof(TKEY));
            file.write((char *) &(it->deleted), sizeof(Qentry::deleted));
        }
        file.close();

        snprintf(filename, 512, "%s/hibernate.buckets", _swappypath);
        file.open(filename, std::ios::out | std::ios::binary);
        length = _buckets.size();
        file.write((char *) &length, sizeof(Id));
        for (auto it = _buckets.begin(); it != _buckets.end(); ++it) {
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
        std::vector<Fid> candidates;
        std::mutex m;
        Fingerprint fp = getFingerprint(key);
        fsearchSuccess = false;
        
        std::for_each (std::execution::par, _buckets.begin(), _buckets.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                
                if ( (key < finfo.minimum) || (key > finfo.maximum) ) {
                    // key is smaller then the smallest or bigger than the biggest
                    m.lock();
                    ++statistic.rangeSaysNo;
                    m.unlock();
                } else {
                    // check bloom (is it possible, that this key is in that file?)
                    bool success = true;
                    for (auto b : fp) {
                        if (finfo.bloomMask[b] == false) {
                            success = false;
                            m.lock();
                            ++statistic.bloomSaysNotIn;
                            m.unlock();
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
                workers.push_back( new std::thread(&SwappyItems::loadFromFile, this, candidates[i], key) );
            }
            // wait for workers
            for (auto w : workers) if (w->joinable()) w->join();
            // cleanup
            for (auto w : workers) delete w;
        }
        
        if (fsearchSuccess) return true;
        
        if (!fsearchSuccess && candidates.size() > 0) {
            // many candidates, but finaly not found in file
            ++statistic.rangeFails;
        }
        return false;
    }

    /**
     * search a key in a file: LOAD
     *
     * if key exists, then load the file into _ramlist (did not delete the file, but
     * file may be overwritten), may save old data in a new file (swap).
     *
     * @param fid the index of the file (and its bitmask id)
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
        snprintf(filename, 512, "%s/%" PRId32 ".bucket", _swappypath, fid);
        std::ifstream file(filename, std::ios::in | std::ios::binary);
        
        for (Id c = 0; c < EACHFILE; ++c) {
            std::vector<TKEY> vecdata(0);
            file.read((char *) &loadedKey, sizeof(TKEY));
            file.read((char *) &loadedValue, sizeof(TVALUE));
            // stored vector?
            file.read((char *) &length, sizeof(Id));
            for (Id j=0; j<length; ++j) {
                file.read((char *) &loadedKey2, sizeof(TKEY));
                vecdata.push_back(loadedKey2);
            }
            temp[loadedKey] = std::make_pair(loadedValue, vecdata);

            if (loadedKey == key) {
                result = true;
                mu_FsearchSuccess.lock();
                fsearchSuccess = true;
                ++statistic.fileLoads;
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
            TKEY omin = _buckets[fid].minimum;
            TKEY omax = _buckets[fid].maximum;
            
            // ok, key exist. clean mask now, because we do not
            // need the (still undeleted) file anymore
            _buckets[fid].minimum = 0;
            _buckets[fid].maximum = 0;

            // we try to make place for the filecontent in RAM ...
            maySwap(true);
            
            // ... and load the stuff
            for (auto key_val : temp) {
                _ramList[key_val.first] = key_val.second;
                _mru.push_back(Qentry{key_val.first,false});
            }
            // update ram min/max
            if (omax > _ramMaximum) _ramMaximum = omax;
            if (omin < _ramMinimum) _ramMinimum = omin;
        }
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
            if ( _mru.size() > ( (RAMSIZE+2) * EACHFILE*OLDIES) ) {
                Order newMRU;
                // remove some deleted items
                for (auto it = _mru.begin(); it != _mru.end(); ++it) {
                    if (it->deleted == false) newMRU.push_back(*it);
                }
                _mru = newMRU;
            }
            return;
        }
        
        Detail detail{0, 0};
        Fingerprint fp;
        
        std::map<TKEY,Data> temp; // map is sorted by key!
        bool success;
        
        ++statistic.swaps;

        // remove old items from front and move them into temp
        for (pos = 0; pos < (EACHFILE*OLDIES); ) {
            Qentry qe = _mru.front();
            _mru.pop_front();
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
                // store bucket
                detail.fid = pos;
                detail.maximum = it->first;
                _buckets[pos] = detail;
                file.close();
            }
        }
        ramMinMax();
    }
    
    /**
     * updates the min and max value from the ram keys
     */
    void ramMinMax(void) {
        auto result = std::minmax_element(std::execution::par, _ramList.begin(), _ramList.end(), [](auto lhs, auto rhs) {
            return (lhs.first < rhs.first); // compare the keys
        });
        //                    .------------ the min
        //                    v    v------- the key of the unordered_map entry
        _ramMinimum = result.first->first;
        //                    v------------ the max
        _ramMaximum = result.second->first;
        
    }

// Each stuff ---------------------------------------------------------------------------------------------------

public:
    
    /**
     * This "for_each" catches all items and runs a function on each item.
     * If this function returns true, the "for_each" stops.
     * 
     * @param back is pointer to the value, were the each loop breaks
     * @param foo is a lambda function, which gets each key and value(pair). if foo return true, the each-loop breaks
     * @return if it is false, the loop did not break
     */
    bool each (Data & back, std::function<bool(TKEY, Data &)> foo) {
        typename Ram::iterator it;
        std::set<Fid> important;
                
        // run in RAM
        /// @todo foreach multithread possible?
        for (it = _ramList.begin(); it != _ramList.end(); ++it) {
            if (foo(it->first, it->second)) {
                back = it->second;
                return true;
            }
        }
        // store all undeleted buckets, because they have the other items
        std::for_each (_buckets.begin(), _buckets.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                important.insert(finfo.fid);
            }
        });
        
        for (Fid fid : important) {
            // 1.1) load into temp each bucket to get all keys of the file
            Ram temp;
            std::unordered_set<TKEY> keys;
            Id length;
            TKEY loadedKey;
            TKEY loadedKey2;
            TVALUE loadedValue;

            char filename[512];
            snprintf(filename, 512, "%s/%" PRId32 ".bucket", _swappypath, fid);
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
            // 1.2) store possible min/max for ram
            TKEY omin = _buckets[fid].minimum;
            TKEY omax = _buckets[fid].maximum;
            // 2) mark bucket file as empty --------------------- 
            _buckets[fid].minimum = 0;
            _buckets[fid].maximum = 0;
            // 3.1) may swap .... ---------- 
            maySwap(true);
            // ... and load temp into ram ----------
            for (auto key_val : temp) {
                _ramList[key_val.first] = key_val.second;
                _mru.push_back(Qentry{key_val.first,false});
            }
            // 3.2) update ram min/max
            if (omax > _ramMaximum) _ramMaximum = omax;
            if (omin < _ramMinimum) _ramMinimum = omin;

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
    
    /**
     * get a item (for use in a "each-loop" in the same SwappyItems instance)
     * 
     * this method never touches the MRU queue and never swaps or load data into RAM.
     * It does not create new items.
     *
     * @param back the value as copy
     * @param key the unique key
     * @param foo the function you wish to apply to the data
     * 
     * @return true, if exists
     */
    bool apply (Data & back, const TKEY & key, std::function<void(Data *)> foo) {
        
        try {
            back = _ramList.at(key);
            foo(&back);
            _ramList[key] = back;
            return true;
            
        } catch (const std::out_of_range & oor) {
            
            std::set<Fid> candidates;
            std::mutex m;
            Fingerprint fp = getFingerprint(key);
            
            std::for_each (std::execution::par, _buckets.begin(), _buckets.end(), [&](Detail finfo) {
                if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                    
                    if ( (key < finfo.minimum) || (key > finfo.maximum) ) {
                        // key is smaller then the smallest or bigger than the biggest
                        m.lock();
                        ++statistic.rangeSaysNo;
                        m.unlock();
                    } else {
                        // check bloom (is it possible, that this key is in that file?)
                        bool success = true;
                        for (auto b : fp) {
                            if (finfo.bloomMask[b] == false) {
                                success = false;
                                m.lock();
                                ++statistic.bloomSaysNotIn;
                                m.unlock();
                                break;
                            }
                        }
                        if (success) {
                            m.lock();
                            candidates.insert(finfo.fid);
                            m.unlock();
                        }
                    }
                }
            });
            
            bool success = false;
            Fid successFid = 0;
            std::streampos startpos;
            std::map<TKEY,Data> temp; // map is sorted by key!
            char filename[512];
                            
            for (auto fid : candidates) {

                Id length;
                TKEY loadedKey;
                TKEY loadedKey2;
                TVALUE loadedValue;
                
                snprintf(filename, 512, "%s/%" PRId32 ".bucket", _swappypath, fid);
                std::ifstream file(filename, std::ios::in | std::ios::binary);

                for (Id c = 0; c < EACHFILE; ++c) {
                    if(!success) startpos = file.tellg();
                    file.read((char *) &loadedKey, sizeof(TKEY));

                    if (loadedKey == key) {
                        success = true;
                        successFid = fid;
                        
                        std::vector<TKEY> vecdata(0);
                        file.read((char *) &loadedValue, sizeof(TVALUE));
                        // stored vector?
                        file.read((char *) &length, sizeof(Id));
                        for (Id j=0; j<length; ++j) {
                            file.read((char *) &loadedKey2, sizeof(TKEY));
                            vecdata.push_back(loadedKey2);
                        }
                        // remember data and end position
                        back = std::make_pair(loadedValue, vecdata);
                    } else {
                        if (success) {
                            // store data from behind the relevant key-value pair
                            std::vector<TKEY> vecdata(0);
                            file.read((char *) &loadedValue, sizeof(TVALUE));
                            // stored vector?
                            file.read((char *) &length, sizeof(Id));
                            for (Id j=0; j<length; ++j) {
                                file.read((char *) &loadedKey2, sizeof(TKEY));
                                vecdata.push_back(loadedKey2);
                            }
                            // remember data
                            temp[loadedKey] = std::make_pair(loadedValue, vecdata);
                        } else {
                            // ignore
                            /// @todo a better way to set the filepointer to the next key-value pair!
                            file.read((char *) &loadedValue, sizeof(TVALUE));
                            // stored vector?
                            file.read((char *) &length, sizeof(Id));
                            for (Id j=0; j<length; ++j) file.read((char *) &loadedKey2, sizeof(TKEY));
                        }
                    }
                }
                file.close();
                // do not load other candidates, because we find the key!
                if (success) break;
            }

            if (success) {
                // apply lambda function to the data with relevant key
                foo(&back);
                temp[key] = back;
                
                // reopen file to renew data
                snprintf(filename, 512, "%s/%" PRId32 ".bucket", _swappypath, successFid);
                //printf("%s\n", filename);
                std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);
                
                // got to place, where we have to renew
                file.seekg(startpos);

                // overwrite data to the file
                typename std::map<TKEY,Data>::iterator it;
                
                for (it=temp.begin(); it!=temp.end(); ++it) {
                    // store data
                    file.write((char *) &(it->first), sizeof(TKEY));
                    file.write((char *) &(it->second.first), sizeof(TVALUE));
                    
                    // store vector?
                    Id le = it->second.second.size();
                    file.write((char *) &le, sizeof(Id));
                    for (Id j=0; j<le; ++j) {
                        file.write((char *) &(it->second.second[j]), sizeof(TKEY));
                    }
                }
                file.close();
                
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
    }


};

#endif
