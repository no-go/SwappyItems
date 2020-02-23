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

/**
 * a 0 as Key means, that this key/value pair is deleted, thus
 * never use 0 as a key! this data may be ignored sometimes.
 */
template <
    class TKEY, class TVALUE,
    int EACHFILE = 16384, int OLDIES = 5, int RAMSIZE = 3, int BLOOMBITS = 4, int MASKLENGTH = (2* 4*16384)
>
class SwappyItems {

    struct Detail {
        TKEY minimum;
        TKEY maximum;
        std::vector<bool> bloomMask;
    };

    typedef uint32_t                               Id;
    typedef uint32_t                              Fid; // File id

    typedef uint32_t                              Bid;
    typedef std::set<Bid>                 Fingerprint;

    typedef std::unordered_map<TKEY,TVALUE>       Ram;
    typedef std::vector<Detail>                Ranges;
    typedef std::deque<TKEY>                    Order;

    // the item store in RAM
    Ram _ramList;

    // a priority queue for the keys
    Order _prios;

    // for each filenr we store min/max value to filter
    Ranges _ranges;

    uint64_t _counting = 0;

    int _swappyId = 0;
    const char _prefix[7] = "./temp"; // limit 120 chars (incl. 2 numbers and ".bin")
    
public:

    struct {
        uint64_t updates = 0;

        uint64_t bloomSaysFresh = 0;
        uint64_t bloomFails = 0;

        uint64_t rangeSaysNo = 0;
        uint64_t rangeFails = 0;

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
            auto it = std::find(_prios.begin(), _prios.end(), key);
            *it = 0; // overwrite the key with zero
            _prios.push_back(key);
            _ramList[key] = value;
            ++statistic.updates;
            return false;
        } else {
            // key is new
            maySwap();
            ++_counting;
            _prios.push_back(key);
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
            auto it = std::find(_prios.begin(), _prios.end(), key);
            *it = 0; // overwrite the key with zero
            _prios.push_back(key);
            return &(_ramList[key]);
        } else {
            return nullptr;
        }
    }

    /**
     * with parameter true you get the number of items in ram
     * 
     * @todo add both to statistics ?
     */
    uint64_t size(bool ramOnly = false) {
        if (ramOnly) return _ramList.size();
        return _counting;
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
        _counting = 0;

        statistic.updates = 0;

        statistic.bloomSaysFresh = 0;
        statistic.rangeSaysNo = 0;
        statistic.rangeFails = 0;

        statistic.fileLoads = 0;
        
        _swappyId = swappyId;
        char filename[120];
        snprintf(filename, 120, "%s%d", _prefix, _swappyId);
        if (filesys::exists(filename)) {
            fprintf(stderr, "folder %s still exists! Please delete or rename it!\n", filename);
            exit(0);
        } else {
            filesys::create_directory(filename);
        }
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
        bool success;
        std::set<Fid> candidates;
        Fingerprint fp = getFingerprint(key);
        
        // pos as index in ranges
        for (Fid fid = 0; fid < _ranges.size(); ++fid) {
            if ( (key < _ranges[fid].minimum) || (key > _ranges[fid].maximum) ) {
                // key is smaller then the smallest or bigger than the biggest
                ++statistic.rangeSaysNo;
            } else {
                // check bloom (is it possible, that this key is in that file?)
                success = true;
                for (auto b : fp) {
                    if (_ranges[fid].bloomMask[b] == false) {
                        success = false;
                        ++statistic.bloomSaysFresh;
                        break;
                    }
                }
                if (success) candidates.insert(fid);
            }
        }

        success = false;
        for (Fid fid : candidates) {
            success = loadFromFile(fid, key);
            if (success) {
                break; // the key was loaded into ram, because the file has it
            }
        }

        if (success == false) {
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
    bool loadFromFile (const Fid & fid, const TKEY & key) {
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
            if (loadedKey == key) result = true;
            temp[loadedKey] = loadedVal;
        }
        file.close();

        if (result == false) return false; // key not exists

        // ok, key exist. clean mask now, because we do not
        // need the (still undeleted) file anymore
        _ranges[fid] = Detail{0, 0};

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
        Detail detail{0, 0};
        Fingerprint fp;
        
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
                    if (_ranges[fid].minimum == 0 && _ranges[fid].maximum == 0) {
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
                detail.maximum = it->first;
                _ranges[pos] = detail;
                file.close();
            }
        }
    }
};

#endif
