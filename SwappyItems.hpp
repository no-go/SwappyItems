#ifndef __SWAPPY_ITEMS_HPP__
#define __SWAPPY_ITEMS_HPP__ 1

/// @todo a 2nd thread to cleanup the deleted entries in _prios deque

// we need it for PRId32 in snprintf
#define __STDC_FORMAT_MACROS

#include <inttypes.h>  // uintX_t stuff
#include <fstream>     // file operations
#include <iostream>    // sizeof and ios namespace
#include <cstdio>      // snprintf for filename
#include <algorithm>   // find
#include <stdexcept>   // std::out_of_range

#include <cstdlib>     // srand, rand
#include <tuple>       // to return some hash values

#include <vector>         // store file information
#include <set>            // a set of candidate files
#include <utility>        // pair
#include <unordered_map>  // for a key->data
#include <deque>          // for key order (prio)
#include <map>            // for store keys in a sorted way

/**
 * a 0 as Key means, that this key/value pair is deleted, thus
 * never use 0 as a key! this data may be ignored sometimes.
 */
template <class TKEY, class TVALUE, int EACHFILE = 16384, int OLDIES = 4, int RAMSIZE = 3>
class SwappyItems {

    typedef uint32_t                               Id;
    typedef uint32_t                              Fid; // File id

    typedef uint32_t                              Bid;
    typedef std::tuple<Bid,Bid,Bid,Bid>   Fingerprint;
    typedef std::set<Bid>                       Bloom;
    typedef std::unordered_map<Bid,Bloom>      Blooms;

    typedef std::unordered_map<TKEY,TVALUE>       Ram;
    typedef std::vector<std::pair<TKEY,TKEY> > Ranges;
    typedef std::deque<TKEY>                    Order;

    uint64_t _counting = 0;

    // the item store in RAM
    Ram _ramList;

    // for each filenr we store min/max value to filter
    Ranges _ranges;

    // a priority queue for the keys
    Order _prios;

    // Indicators to detect new keys
    Bloom  _indicator1;
    Blooms _indicator2;


public:

    uint64_t updates = 0;

    uint64_t bloomSaysFresh = 0;
    uint64_t bloomFails = 0;

    uint64_t rangeSaysNo = 0;
    uint64_t rangeFails = 0;

    uint64_t fileLoads = 0;

    /**
     * it is a quasi copy of set()
     */
    TVALUE & operator[] (TKEY key) {
        // still exists?
        bool isLoaded = false;
        Fingerprint fp = getFingerprint(key);
        bool maybe = mayExist(fp);
        if (maybe) {
            // key may exists, we try to load it
            isLoaded = load(key);
        } else {
            ++bloomSaysFresh;
        }

        if (isLoaded) {
            // just update
            auto it = std::find(_prios.begin(), _prios.end(), key);
            *it = 0; // overwrite the key with zero
            _prios.push_back(key);
            return _ramList[key];
        } else {
            // key is new
            maySwap();
            ++_counting;
            _prios.push_back(key);
            //_ramList[key] = value;

            if (maybe == false) {
                _indicator1.insert(std::get<0>(fp));
                _indicator2[std::get<1>(fp)].insert(std::get<2>(fp));
                _indicator2[std::get<1>(fp)].insert(std::get<3>(fp));
            } else {
                // bloom think, the key may exist, but the key does not exist
                ++bloomFails;
            }
            return _ramList[key];
        }
    }

    /**
     * set (create or update)
     *
     * @return true if it is new
     */
    bool set (TKEY key, TVALUE value) {
        // still exists?
        bool isLoaded = false;
        Fingerprint fp = getFingerprint(key);
        bool maybe = mayExist(fp);
        if (maybe) {
            // key may exists, we try to load it
            isLoaded = load(key);
        } else {
            ++bloomSaysFresh;
        }

        if (isLoaded) {
            // just update
            auto it = std::find(_prios.begin(), _prios.end(), key);
            *it = 0; // overwrite the key with zero
            _prios.push_back(key);
            _ramList[key] = value;
            ++updates;
            return false;
        } else {
            // key is new
            maySwap();
            ++_counting;
            _prios.push_back(key);
            _ramList[key] = value;

            if (maybe == false) {
                _indicator1.insert(std::get<0>(fp));
                _indicator2[std::get<1>(fp)].insert(std::get<2>(fp));
                _indicator2[std::get<1>(fp)].insert(std::get<3>(fp));
            } else {
                // bloom think, the key may exist, but the key does not exist
                ++bloomFails;
            }
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
        Fingerprint fp = getFingerprint(key);
        if (mayExist(fp) == false) {
            ++bloomSaysFresh;
            return nullptr;
        }
        bool isLoaded = load(key);
        if (isLoaded) {
            auto it = std::find(_prios.begin(), _prios.end(), key);
            *it = 0; // overwrite the key with zero
            _prios.push_back(key);
            return &(_ramList[key]);
        } else {
            return nullptr;
        }
    }

    uint64_t size(bool ramOnly = false) {
        if (ramOnly) return _ramList.size();
        return _counting;
    }

    SwappyItems () {
        _counting = 0;
        updates = 0;

        bloomSaysFresh = 0;
        bloomFails = 0;

        rangeSaysNo = 0;
        rangeFails = 0;

        fileLoads = 0;
    }

private:

    /**
     * this is a not mathematical exact improvement
     */
    Fingerprint getFingerprint (const TKEY & key) {
        srand(key);
        return std::make_tuple(
            rand()%(EACHFILE * OLDIES),
            rand()%(EACHFILE * OLDIES),
            rand()%(EACHFILE>>2),
            rand()%(EACHFILE>>2)
        );
    }

    bool mayExist (const Fingerprint & fp) {
        Bloom indi_3_4;

        if (_indicator1.find(std::get<0>(fp)) == _indicator1.end() ) return false;

        try {
            indi_3_4 = _indicator2.at(std::get<1>(fp));
        } catch (const std::out_of_range & oor) {
            return false;
        }

        if (indi_3_4.find(std::get<2>(fp)) == indi_3_4.end() ) return false;
        if (indi_3_4.find(std::get<3>(fp)) == indi_3_4.end() ) return false;

        return true;
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
        bool success = false;
        std::set<Fid> candidates;

        // pos as index in ranges
        for (Fid fid = 0; fid < _ranges.size(); ++fid) {
            if ( (key < _ranges[fid].first) || (key > _ranges[fid].second) ) {
                // key is smaller then the smallest or bigger than the biggest
                ++rangeSaysNo;
            } else {
                candidates.insert(fid);
            }
        }

        for (Fid fid : candidates) {
            success = loadFromFile(fid, key);
            if (success) {
#ifdef DEBUG
                printf(
                    "%" PRId64 " is in %" PRId32 ".bin\n",
                    key, fid
                );
#endif
                break; // the key was loaded into ram, because the file has it
            }
        }

        if (success == false) {
            if (candidates.size() > 0) {
                // many candidates, but finaly not found in file
                ++rangeFails;
            }
            return false;
        } else {
            ++fileLoads;
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
    bool loadFromFile (const Fid & fid, const TKEY & key) {
        // @todo Items are sorted in file! binary search?

        Ram temp;
        TKEY loadedKey;
        TVALUE loadedVal;
        bool result = false;

        char filename[60];
        snprintf(filename, 60, "./temp/%" PRId32 ".bin", fid);
        std::ifstream file(filename, std::ios::in | std::ios::binary);

        for (Id c = 0; c < EACHFILE; ++c) {
            file.read((char *) &loadedKey, sizeof(TKEY));
            file.read((char *) &loadedVal, sizeof(TVALUE));
            if (loadedKey == key) result = true;
            // do not load deleted stuff
            if (loadedKey>0) temp[loadedKey] = loadedVal;
            ++c;
        }
        file.close();

        if (result == false) return false; // key not exists

        // ok, key exist. clean mask now, because we do not
        // need the (still undeleted) file anymore
        _ranges[fid] = std::make_pair(0,0);

        // we try to make place for the filecontent in RAM ...
        maySwap(true);
        // ... and load the stuff
        for (auto key_val : temp) {
            _ramList[key_val.first] = key_val.second;
            _prios.push_back(key_val.first);
        }
        return true;
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
        TKEY smallest;
        std::map<TKEY,TVALUE> temp; // map is sorted by key!
        bool success;

        // remove old items from front and move them to temp
        for (pos = 0; pos < (OLDIES*EACHFILE); ) {
            TKEY key = _prios.front();
            _prios.pop_front();
            if (key > 0) { // HINT
                temp[key] = _ramList[key];
                _ramList.erase(key);
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
                    if (_ranges[fid].first == 0 && _ranges[fid].second == 0) {
                        pos = fid;
                        success = true;
                        break; // we found an empty Range
                    }
                }

                if (success == false) {
                    pos = _ranges.size();
                    _ranges.push_back(std::make_pair(0,0)); // add an empty dummy Range
                }

                char filename[60];
                snprintf(filename, 60, "./temp/%" PRId32 ".bin", pos);
                file.open(filename, std::ios::out | std::ios::binary);
                smallest = it->first;
            }

            // store data
            file.write((char *) &(it->first), sizeof(TKEY));
            file.write((char *) &(it->second), sizeof(TVALUE));

            ++written;

            if ((written%(EACHFILE)) == 0) {
                // store range
                _ranges[pos] = std::make_pair(
                    smallest,
                    it->first
                );
                file.close();
            }
        }

        // because we do not write "deleted keys", this could happend.
        // sometimes files store less than EACHFILE items
        // HINT: NO, because of "if (key > 0) {" these keyes are not in temp
        if ((written%(EACHFILE)) != 0) {
            // store range
            _ranges[pos] = std::make_pair(
                smallest,
                it->first
            );
            file.close();
        }
    }
};

#endif
