#ifndef __P_KUH_HPP__
#define __P_KUH_HPP__ 1

#include <inttypes.h>  // uintX_t stuff
#include <iostream>    // sizeof and ios namespace
#include <algorithm>   // find, minmax
#include <stdexcept>   // std::out_of_range
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

#include <tuple>

/**
 * This Code is like SwappyItems, but additionaly...
 * 
 * - every Item should have a prio value (uint64_t)
 * - pop() gets and removes Item with MIN prio
 * - top() gets Item with MIN prio
 * - update() change prio of EXISTING Item
 * - each() and apply() are removed, because prio changes in it occurs to much heap rebuilding
 * 
 * CODE HAS STILL BUGS!
 * 
 * ## Details
 * 
 * - 2 phase pairing heap
 * - the heap magic is in `update()`, `del()` and the private methode `insert()`
 * - additional to a Item(key, value, vector of keys, prio) is stored (private):
 *   - a parent key
 *   - a 2nd vector of keys to "sibling nodes"
 * - a head key and its data (outside of _ramList and files on DISK)
 * 
 * ## Interesting
 * 
 * - most recently used elements are still in RAM
 * - elements with less access in PRIO updates may Swap to DISK (= deep heap content leaves the RAM!)
 * 
 * @tparam TKEY is the type for the key, which identifies the Item.
 * @tparam TVALUE is the type for the value, which has the data of the Item.
 * 
 * @tparam EACHFILE is number of Items in each swap file. Please choose a 2^n number like 65536 or 2048
 * @tparam OLDIES is number of files, if a swap occurs. Please choose a 2n number above 6
 * @tparam RAMSIZE if _ramList has more than RAMSIZE x OLDIES x EACHFILE Items, a swap occurs.
 * @tparam BLOOMBITS is the number of fingerprint bits for a key, which are set in a MASK for each file. Stupid value: 1 Bit at MASKLENGTH = EACHFILE; possible but not very meaningful: 4 Bits at MASKLENGTH = 4x EACHFILE
 * @tparam MASKLENGTH is the MASK size of a file, where the fingerprint bits are set. A good choice to get 50% filling: 2x BLOOMBITS x EACHFILE
 */
template <
    class TKEY,
    class TVALUE,
        
    int EACHFILE = 16384,
    int OLDIES = 8,
    int RAMSIZE = 3,
    int BLOOMBITS = 4,
    int MASKLENGTH = (2* 4*16384)
>
class Pkuh {

public:
    // only for get, top, pop !
    //                 nodedata          neighbors      prio -----------------------------START TYPES
    typedef std::tuple<  TVALUE, std::vector<TKEY>, uint64_t> Data;
    
    struct statistic_s {
        uint64_t size    = 0;
        uint64_t fileKB  = 0;
        uint64_t queue   = 0;
        
        uint64_t priochanges = 0;
        uint64_t updates     = 0;
        uint64_t deletes     = 0;

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
//                     nodedata          neighbors      prio    parent            sibling
    typedef std::tuple<  TVALUE, std::vector<TKEY>, uint64_t,     TKEY, std::vector<TKEY> > InternalData;

    TKEY      _KEYMAX;
    uint64_t _PRIOMAX;

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
    
    typedef std::unordered_map<TKEY,InternalData>      Ram;
    typedef std::vector<Detail>                    Buckets;
    typedef std::deque<Qentry>                       Order;

// ----------------------------------------------------------------------------------------END TYPES

    // the item store in RAM
    Ram _ramList;
    TKEY _ramMinimum;
    TKEY _ramMaximum;
        
    // a queue for the keys (most recently used)
    Order _mru;
    
    // The actual key and the Data of the Priority Queue Head
    TKEY         _headKey;
    InternalData _headData;

    // for each filenr we store min/max value and a bloom mask to filter
    Buckets _buckets;
    struct statistic_s statistic;

    const char _prefix[7] = "./pkuh";
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
        return set(key, value, std::vector<TKEY>(0));
    }
    
    /**
     * set (create or update) with additional vector of keys
     *
     * @return true if it is new
     */
    bool set (TKEY key, TVALUE value, std::vector<TKEY> refs) {
        if (_headKey == _KEYMAX) {
            // is head empty? = Priority Queue is empty!
            
            _headKey = key;
            _headData = std::make_tuple(
                value,
                refs,
                _PRIOMAX,
                key,                      // inital it is its own parent
                std::vector<TKEY>(0)
            );
            ++statistic.size;
            if (_headKey > _ramMaximum) _ramMaximum = _headKey;
            if (_headKey < _ramMinimum) _ramMinimum = _headKey;
            return true;
            
        } else if (key == _headKey) {
            // is key = top?

            _headData = std::make_tuple(
                value,
                refs,
                std::get<2>(_headData),
                std::get<3>(_headData),
                std::get<4>(_headData)
            );
            
            ++statistic.updates;
            return false;
            
        } else if (load(key)) {
            // just update
            // reverse search should be faster
            auto it = std::find_if(std::execution::par, _mru.rbegin(), _mru.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            it->deleted = true;
            _mru.push_back(Qentry{key,false});
            std::get<0>(_ramList[key]) = value;
            std::get<1>(_ramList[key]) = refs;
            ++statistic.updates;
            return false;
        } else {
            // key is new
            maySwap();
            ++statistic.size;
            _mru.push_back(Qentry{key,false});
            _ramList[key] = std::make_tuple(
                value,
                refs,
                _PRIOMAX,
                _headKey,
                std::vector<TKEY>(0)
            );
            std::get<4>(_headData).push_back(key);
            
            // update min/max of RAM
            if (key > _ramMaximum) _ramMaximum = key;
            if (key < _ramMinimum) _ramMinimum = key;
            return true;
        }
    }

    /**
     * update prio
     * 
     * @todo please refactor redundant code here!
     * @return false if it does not exists
     */
    bool update (TKEY key, uint64_t prio) {
        
        if (key == _headKey) {
            
            if (prio <= std::get<2>(_headData)) {
                std::get<2>(_headData) = prio;
                ++statistic.priochanges;
                return true;
            
            } else {
                // after delete we will insert the head like a normal new element
                InternalData element = std::make_tuple(
                    std::get<0>(_headData),
                    std::get<1>(_headData),
                    prio,
                    key,                    // inital it is its own parent
                    std::vector<TKEY>(0)
                );
                
                bool noUglyBug = del(_headKey);         // size--
                --statistic.deletes;
                insert(key, element);  // size++
                ++statistic.priochanges;
                return noUglyBug;
            }
        }
        
        if (load(key)) {
            
            if (std::get<2>(_ramList[key]) == prio) return true;
            
            InternalData element = std::make_tuple(
                std::get<0>(_ramList[key]),
                std::get<1>(_ramList[key]),
                prio,
                key,                      // inital it is its own parent
                std::vector<TKEY>(0)
            );
            
            bool noUglyBug = del(key);   // size--
            --statistic.deletes;
            insert(key, element);        // size++
            ++statistic.priochanges;
            return noUglyBug;
        } else {
            return false;
        }
    }

    /**
     * get a item
     *
     * @todo please refactor redundant code here!
     * @param key the unique key
     * @param result the reference, where you get a copy of the stored data
     * @return false, if key not exists
     */
    bool get (const TKEY & key, Data & result) {
        if (key == _headKey) {
            // is key = top?
            std::get<0>(result) = std::get<0>(_headData);
            std::get<1>(result) = std::get<1>(_headData);
            std::get<2>(result) = std::get<2>(_headData);
            return true;
            
        } else if (load(key)) {
            // reverse search should be faster
            auto it = std::find_if(std::execution::par, _mru.rbegin(), _mru.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            // every get fills the queue with "deleted = true" on this key, thus older entries are never equal false
            it->deleted = true;
            _mru.push_back(Qentry{key,false});
            std::get<0>(result) = std::get<0>(_ramList[key]);
            std::get<1>(result) = std::get<1>(_ramList[key]);
            std::get<2>(result) = std::get<2>(_ramList[key]);
            return true;
        } else {
            return false;
        }
    }
    
    /**
     * get the top item
     *
     * @param resultkey has the key of the head
     * @param result has the reference, where you get a copy of the stored data
     * @return false, if top not exists
     */
    bool top (TKEY & resultkey, Data & result) {
        if (_headKey != _KEYMAX) {
            resultkey = _headKey;
            std::get<0>(result) = std::get<0>(_headData);
            std::get<1>(result) = std::get<1>(_headData);
            std::get<2>(result) = std::get<2>(_headData);
            return true;
        } else {
            return false;
        }
    }
    
    /**
     * get and delete the top item
     *
     * @param resultkey has the key of the head
     * @param result has the reference, where you get a copy of the stored data
     * @return false, if top not exists
     */
    bool pop (TKEY & resultkey, Data & result) {
        bool back = top(resultkey, result);
        if (back) del(resultkey);
        return back;
    }
    
    /**
     * delete a item
     *
     * @param key the unique key
     * @return false if item not exists
     */
    bool del (const TKEY & key) {
        InternalData element; // = remember the datas from old element
        
        std::vector<TKEY> siblings;
        std::vector<TKEY> newsiblings;
        std::vector<TKEY> winners;
        TKEY parent;
        TKEY champion;
        uint64_t prio1;
        uint64_t prio2;

        // is key = top?
        if (key == _headKey) {
            
            element = std::make_tuple(
                std::get<0>(_headData),   // user data
                std::get<1>(_headData),   // user data (vector of keys)
                std::get<2>(_headData),   // user data prio
                std::get<3>(_headData),   // parent
                std::get<4>(_headData)    // siblings
            );
            
            if (std::get<4>(_headData).size() == 0) {
                // empty now!!
                statistic.size = 0;
                _headKey = _KEYMAX;                             // initial is max key the "empty"
                std::get<1>(_headData) = std::vector<TKEY>(0);  // empty Vector (in userspace)
                std::get<2>(_headData) = _PRIOMAX;              // maximal possible prio (in a min heap)
                std::get<3>(_headData) = _KEYMAX;               // head node on empty heap has itself as parent
                std::get<4>(_headData) = std::vector<TKEY>(0);  // empty Vector of sibling (not userspace)

                _ramMinimum = std::numeric_limits<TKEY>::max();
                _ramMaximum = std::numeric_limits<TKEY>::min();              
                return true;
            }

        } else {
            // not exists?
            if (load(key) == false) return false;

            // reverse search should be faster
            auto it = std::find_if(std::execution::par, _mru.rbegin(), _mru.rend(), 
                [key] (const Qentry & qe) {
                    return (qe.key == key) && (qe.deleted == false);
                }
            );
            it->deleted = true;
            // we really have to delete it in ram, because load() search key initialy in ram!
            element = std::make_tuple(
                std::get<0>(_ramList[key]),   // user data
                std::get<1>(_ramList[key]),   // user data (vector of keys)
                std::get<2>(_ramList[key]),   // user data prio
                std::get<3>(_ramList[key]),   // parent
                std::get<4>(_ramList[key])    // siblings
            );
            _ramList.erase(key);
        }

        if (key != _headKey) {
            // remove key from siblings of parent
            
            parent = std::get<3>(element);
            // but if parent is head, then we need to update siblings from _headData!
            if (parent == _headKey) {
                siblings = std::get<4>(_headData);
                for (auto n : siblings) {
                    if (n != key) newsiblings.push_back(n);
                }
                std::get<4>(_headData) = newsiblings;

            } else {
                /// @todo maybe to much load and swapping
                if (load(parent, true) == false) {
                    /// @todo Processing error! uglyBug + Parent not exist!!!!!
                    return false;
                }
                siblings = std::get<4>(_ramList[parent]);
                for (auto n : siblings) {
                    if (n != key) newsiblings.push_back(n);
                }
                std::get<4>(_ramList[parent]) = newsiblings;                
            }
        }
        
        // build pairs (1. phase) --------------------------------------
        siblings = std::get<4>(element);
        for (Id i=0; i < siblings.size(); i+=2) {
            if ((i+1) >= siblings.size()) {
                // a single element ...
                winners.push_back(siblings[i]);
            } else {
                /// @todo maybe to much load and swapping
                if (load(siblings[i], true) == false) {
                    /// @todo Processing error! uglyBug = sibling i not exist!!!!!
                    return false;
                }
                /// @todo maybe to much load and swapping
                if (load(siblings[i+1], true) == false) {
                    /// @todo Processing error! uglyBug = sibling i+1 not exist!!!!!
                    return false;
                }
                prio1 = std::get<2>(_ramList[siblings[i]]);
                prio2 = std::get<2>(_ramList[siblings[i+1]]);
                
                if (prio1 < prio2) {
                    winners.push_back(siblings[i]);
                    std::get<3>(_ramList[siblings[i+1]]) = siblings[i];
                    std::get<4>(_ramList[siblings[i]]).push_back(siblings[i+1]);
                } else {
                    winners.push_back(siblings[i+1]);
                    std::get<3>(_ramList[siblings[i]]) = siblings[i+1];
                    std::get<4>(_ramList[siblings[i+1]]).push_back(siblings[i]);
                }
            }
        }

        // build a single tree (2. phase) -----------------------------------
        if (winners.size() > 0) {
            champion = winners[0];
            for (Id i=1; i < winners.size(); ++i) {
                prio1 = std::get<2>(_ramList[winners[i]]);
                prio2 = std::get<2>(_ramList[champion]);
                
                if (prio1 < prio2) {
                    std::get<4>(_ramList[winners[i]]).push_back(champion);
                    std::get<3>(_ramList[champion]) = winners[i];
                    champion = winners[i]; // we got a new minimal champion
                } else {
                    std::get<4>(_ramList[champion]).push_back(winners[i]);
                    std::get<3>(_ramList[winners[i]]) = champion;
                }
            }
            
            // link champion to parent of deleted element
            
            // .. which may be a new head
            if (key == _headKey) {
                _headKey  = champion;              // the winner of all winners
                _headData = std::make_tuple(
                    std::get<0>(_ramList[champion]),   // user data
                    std::get<1>(_ramList[champion]),   // user data (vector of keys)
                    std::get<2>(_ramList[champion]),   // user data prio
                    _headKey,                          // no parent = link to its self!
                    std::get<4>(_ramList[champion])    // siblings
                );
                
                // remove new head from the _ramList
                auto it = std::find_if(std::execution::par, _mru.rbegin(), _mru.rend(), 
                    [champion] (const Qentry & qe) {
                        return (qe.key == champion) && (qe.deleted == false);
                    }
                );
                it->deleted = true;
                _ramList.erase(champion);
            
            } else {
                // the champion has to be linked to the old parent
                std::get<3>(_ramList[champion]) = parent;
                
                // add champion to the parent siblings
                if (parent == _headKey) {
                    // if parent is the head, we have to access _headData instead of _ramList
                    std::get<4>(_headData).push_back(champion);
                } else {
                    std::get<4>(_ramList[parent]).push_back(champion);
                }
            }
        }

        --statistic.size;
        ++statistic.deletes;
        
        if (key == _ramMaximum || key == _ramMinimum) ramMinMax();
        return true;
    }
    
    /**
     * get a struct with many statistic values
     */
    struct statistic_s getStatistic (void) {
        statistic.fileKB = (_buckets.size() * EACHFILE * (sizeof(TKEY) + sizeof(TVALUE))/1000);
        statistic.queue = _mru.size();

        statistic.minKey = std::numeric_limits<TKEY>::max();
        statistic.maxKey = std::numeric_limits<TKEY>::min();
        // @todo makes multithread sense here? maybe collect mins and maxs from threads and get min of them?
        std::for_each (_buckets.begin(), _buckets.end(), [&](Detail finfo) {
            if ( finfo.minimum != finfo.maximum ) { // it is not deleted!
                if (finfo.minimum < statistic.minKey) statistic.minKey = finfo.minimum;
                if (finfo.maximum > statistic.maxKey) statistic.maxKey = finfo.maximum;
            }
        });
        if (_ramMinimum < statistic.minKey) statistic.minKey = _ramMinimum;
        if (_ramMaximum > statistic.maxKey) statistic.maxKey = _ramMaximum;
        
        return statistic;
    }

    /**
     * It trys to wakeup from hibernate files. The id parameter identifies the 
     * Pkuh Store (important for the file path!).
     * 
     * @param swappyId parameter identifies the instance
     */
    Pkuh (int swappyId) {
        _KEYMAX = std::numeric_limits<std::uint64_t>::max();
        _PRIOMAX = std::numeric_limits<std::uint64_t>::max();
        
        _headKey = _KEYMAX;                       // initial is max key the "empty"
        std::get<1>(_headData) = std::vector<TKEY>(0);  // empty Vector (in userspace)
        std::get<2>(_headData) = _PRIOMAX;              // maximal possible prio (in a min heap)
        std::get<3>(_headData) = _KEYMAX;               // head node on empty heap has itself as parent
        std::get<4>(_headData) = std::vector<TKEY>(0);  // empty Vector of sibling (not userspace)
        
        statistic.size   = 0;
        statistic.fileKB = 0;
        statistic.queue  = 0;
        
        statistic.priochanges = 0;
        statistic.updates     = 0;
        statistic.deletes     = 0;

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
                    "# wakeup on items, priochanges, updates, deletes: "
                    "%10ld %10" PRId64 " %10" PRId64 " %10" PRId64 "\n",
                    statistic.size,
                    statistic.priochanges,
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
    ~Pkuh (void) {
        hibernate();
    }

// ------------------------------------------------------------------------------------


private:
    /**
     * join a NEW (!!) Item to the head or replace the head
     * 
     * @param key the unique key
     * @param element the new element with internal tuple
     */
    void insert (TKEY & key, InternalData & element) {
        
        /// @todo do 2 phases if sibling size > OLDIES/2
        
        if (_headKey == _KEYMAX) {
            // Pkuh is empty, because _headKey is not set!
            _headKey = key;
            _headData = std::make_tuple(
                std::get<0>(element),     // user data
                std::get<1>(element),     // user data (vector of keys)
                std::get<2>(element),     // user data prio
                key,                      // inital it is its own parent
                std::vector<TKEY>(0)      // no siblings
            );
            
            
        } else {

            if (std::get<2>(element) < std::get<2>(_headData)) {
                // element prio is smaller => new head!
                
                // move old head to RAM list
                _mru.push_back(Qentry{_headKey,false});
                _ramList[_headKey] = std::make_tuple(
                    std::get<0>(_headData),   // user data
                    std::get<1>(_headData),   // user data (vector of keys)
                    std::get<2>(_headData),   // user data prio
                    key,                      // head gets the new key as parent
                    std::get<4>(_headData)    // siblings
                );
                
                // old head is sibling of new head
                std::get<4>(element).push_back(_headKey);
                
                _headKey = key;
                _headData = std::make_tuple(
                    std::get<0>(element),     // user data
                    std::get<1>(element),     // user data (vector of keys)
                    std::get<2>(element),     // user data prio
                    key,                      // inital it is its own parent
                    std::get<4>(element)      // old head is sibling
                );
                
            } else {
                // move it to RAM list
                _mru.push_back(Qentry{key,false});
                _ramList[key] = std::make_tuple(
                    std::get<0>(element),   // user data
                    std::get<1>(element),   // user data (vector of keys)
                    std::get<2>(element),   // user data prio
                    _headKey,               // head is parent
                    std::vector<TKEY>(0)    // no siblings
                );
                // add to siblings of head
                std::get<4>(_headData).push_back(key);
            }
        }

        ++statistic.size;
        // update min/max of RAM
        if (key > _ramMaximum) _ramMaximum = key;
        if (key < _ramMinimum) _ramMinimum = key;
        if (_headKey > _ramMaximum) _ramMaximum = _headKey;
        if (_headKey < _ramMinimum) _ramMinimum = _headKey;
    }


    /**
     * load all hibernate content into RAM
     * 
     * @todo return false on fail
     */
    bool wakeup (void) {
        char filename[512];
        TKEY loadedKey;
        TKEY loadedKey2;
        TVALUE loadedValue;
        uint64_t loadedPrio;
        TKEY parent;
        std::ifstream file;

        snprintf(filename, 512, "%s/hibernate.ramlist", _swappypath);
        file.open(filename, std::ios::in | std::ios::binary);
        Id length;
        Id lengthVec;

        // read head
        file.read((char *) &_headKey, sizeof(TKEY));
        file.read((char *) &(std::get<0>(_headData)), sizeof(TVALUE));
        file.read((char *) &(std::get<2>(_headData)), sizeof(uint64_t));
        file.read((char *) &(std::get<3>(_headData)), sizeof(TKEY));
        // head stored vector?
        file.read((char *) &lengthVec, sizeof(Id));
        for (Id j=0; j<lengthVec; ++j) {
            file.read((char *) &(std::get<1>(_headData)[j]), sizeof(TKEY));
        }
        // head stored sibling?
        file.read((char *) &lengthVec, sizeof(Id));
        for (Id j=0; j<lengthVec; ++j) {
            file.read((char *) &(std::get<4>(_headData)[j]), sizeof(TKEY));
        }
        
        // read ramlist
        file.read((char *) &length, sizeof(Id));
        for (Id c = 0; c < length; ++c) {
            std::vector<TKEY> vecdata(0);
            std::vector<TKEY> sibling(0);
            file.read((char *) &loadedKey, sizeof(TKEY));
            file.read((char *) &loadedValue, sizeof(TVALUE));
            file.read((char *) &loadedPrio, sizeof(uint64_t));
            file.read((char *) &parent, sizeof(TKEY));
            // stored vector?
            file.read((char *) &lengthVec, sizeof(Id));
            for (Id j=0; j<lengthVec; ++j) {
                file.read((char *) &loadedKey2, sizeof(TKEY));
                vecdata.push_back(loadedKey2);
            }
            // stored sibling?
            file.read((char *) &lengthVec, sizeof(Id));
            for (Id j=0; j<lengthVec; ++j) {
                file.read((char *) &loadedKey2, sizeof(TKEY));
                sibling.push_back(loadedKey2);
            }
            _ramList[loadedKey] = std::make_tuple(loadedValue, vecdata, loadedPrio, parent, sibling);
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
     * dear programmer, catch a strg+c and make a regular delete of Pkuh
     * in your code to do a hibernate!
     */
    void hibernate (void) {
        char filename[512];

        snprintf(filename, 512, "%s/hibernate.ramlist", _swappypath);
        std::ofstream file(filename, std::ios::out | std::ios::binary);
        Id length;

        // store head
        file.write((char *) &_headKey, sizeof(TKEY));
        file.write((char *) &(std::get<0>(_headData)), sizeof(TVALUE));
        file.write((char *) &(std::get<2>(_headData)), sizeof(uint64_t));
        file.write((char *) &(std::get<3>(_headData)), sizeof(TKEY));
        // store head vector?
        length = std::get<1>(_headData).size();
        file.write((char *) &length, sizeof(Id));
        for (Id j=0; j<length; ++j) {
            file.write((char *) &(std::get<1>(_headData)[j]), sizeof(TKEY));
        }
        // store head sibling?
        length = std::get<4>(_headData).size();
        file.write((char *) &length, sizeof(Id));
        for (Id j=0; j<length; ++j) {
            file.write((char *) &(std::get<4>(_headData)[j]), sizeof(TKEY));
        }

        // store ramlist
        length = _ramList.size();
        file.write((char *) &length, sizeof(Id));
        for (auto it = _ramList.begin(); it != _ramList.end(); ++it) {
            file.write((char *) &(it->first), sizeof(TKEY));
            file.write((char *) &(std::get<0>(it->second)), sizeof(TVALUE));
            file.write((char *) &(std::get<2>(it->second)), sizeof(uint64_t));
            file.write((char *) &(std::get<3>(it->second)), sizeof(TKEY));
            // stored vector?
            length = std::get<1>(it->second).size();
            file.write((char *) &length, sizeof(Id));
            for (Id j=0; j<length; ++j) {
                file.write((char *) &(std::get<1>(it->second)[j]), sizeof(TKEY));
            }
            // stored sibling?
            length = std::get<4>(it->second).size();
            file.write((char *) &length, sizeof(Id));
            for (Id j=0; j<length; ++j) {
                file.write((char *) &(std::get<4>(it->second)[j]), sizeof(TKEY));
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
     * @param key the unique key, we want in the RAM
     * @param noReloadSwap set it to true and a reload from _
     *        a file will not force a swap of old stuff
     * @return false, if key not exists
     */
    bool load (const TKEY & key, bool noReloadSwap = false) {
        try {
            _ramList.at(key);
            return true;
        } catch (const std::out_of_range & oor) {
            return loadFromFiles(key, noReloadSwap);
        }
    }

    /**
     * search a key in all (maybe) relevant files and load it to ram
     *
     * @param key the unique key we want to find in the files, because it is not in RAM
     * @param noReloadSwap set it to true and a reload from _
     *        a file will not force a swap of old stuff
     * @return false, if not exists
     */
    bool loadFromFiles (const TKEY & key, bool noReloadSwap = false) {
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
                workers.push_back( new std::thread(&Pkuh::loadFromFile, this, candidates[i], key, noReloadSwap) );
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
     * @param noReloadSwap set it to true and a reload from _
     *        a file will not force a swap of old stuff
     * @ return true, if key is loaded into RAM
     */
    void loadFromFile (Fid fid, TKEY key, bool noReloadSwap = false) {
        Ram temp;
        Id lengthVec;
        TKEY loadedKey;
        TKEY loadedKey2;
        TVALUE loadedValue;
        uint64_t loadedPrio;
        TKEY parent;
        
        bool result = false;
        
        char filename[512];
        snprintf(filename, 512, "%s/%" PRId32 ".bucket", _swappypath, fid);
        std::ifstream file(filename, std::ios::in | std::ios::binary);
        
        for (Id c = 0; c < EACHFILE; ++c) {
            std::vector<TKEY> vecdata(0);
            std::vector<TKEY> sibling(0);
            file.read((char *) &loadedKey, sizeof(TKEY));
            file.read((char *) &loadedValue, sizeof(TVALUE));
            file.read((char *) &loadedPrio, sizeof(uint64_t));
            file.read((char *) &parent, sizeof(TKEY));
            // stored vector?
            file.read((char *) &lengthVec, sizeof(Id));
            for (Id j=0; j<lengthVec; ++j) {
                file.read((char *) &loadedKey2, sizeof(TKEY));
                vecdata.push_back(loadedKey2);
            }
            // stored sibling?
            file.read((char *) &lengthVec, sizeof(Id));
            for (Id j=0; j<lengthVec; ++j) {
                file.read((char *) &loadedKey2, sizeof(TKEY));
                sibling.push_back(loadedKey2);
            }
            temp[loadedKey] = std::make_tuple(loadedValue, vecdata, loadedPrio, parent, sibling);

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

            /**
             * we try to make place for the filecontent in RAM ... = a reloadSwap!
             * noReloadSwap is an optional load() parameter. we use it for loading
             * sibling data additional into _ramList. this blows the RAM a bit up
             * and not replace other parent data from RAM, which may be old.
             */
            if (noReloadSwap == false) maySwap(true);
            
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
        
        std::map<TKEY,InternalData> temp; // map is sorted by key!
        bool success;
        
        ++statistic.swaps;

        // remove old items from front and move them into temp
        for (pos = 0; pos < (EACHFILE*OLDIES); ) {
            /// @todo does this really happend?! or is it a programming bug (because it happend)
            //if (_mru.size() == 0) break;
            
            Qentry qe = _mru.front();
            _mru.pop_front();
            if (qe.deleted == false) {
                temp[qe.key] = std::make_tuple(
                    std::get<0>(_ramList[qe.key]),   // user data
                    std::get<1>(_ramList[qe.key]),   // user data (vector of keys)
                    std::get<2>(_ramList[qe.key]),   // user data prio
                    std::get<3>(_ramList[qe.key]),   // parent
                    std::get<4>(_ramList[qe.key])    // siblings
                );              
                _ramList.erase(qe.key);
                ++pos; // we only count undeleted stuff!
            }
        }

        // run through sorted items
        Id written = 0;
        std::ofstream file;
        typename std::map<TKEY,InternalData>::iterator it;

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
            file.write((char *) &(std::get<0>(it->second)), sizeof(TVALUE));
            file.write((char *) &(std::get<2>(it->second)), sizeof(uint64_t));
            file.write((char *) &(std::get<3>(it->second)), sizeof(TKEY));
            
            // store vector?
            Id lengthVec = std::get<1>(it->second).size();
            file.write((char *) &lengthVec, sizeof(Id));
            for (Id j=0; j<lengthVec; ++j) {
                file.write((char *) &(std::get<1>(it->second)[j]), sizeof(TKEY));
            }
            // stored sibling?
            lengthVec = std::get<4>(it->second).size();
            file.write((char *) &lengthVec, sizeof(Id));
            for (Id j=0; j<lengthVec; ++j) {
                file.write((char *) &(std::get<4>(it->second)[j]), sizeof(TKEY));
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
        if (_ramList.size() > 0) {
            auto result = std::minmax_element(std::execution::par, _ramList.begin(), _ramList.end(), [](auto lhs, auto rhs) {
                return (lhs.first < rhs.first); // compare the keys
            });
            //                    .------------ the min
            //                    v    v------- the key of the unordered_map entry
            _ramMinimum = result.first->first;
            //                    v------------ the max
            _ramMaximum = result.second->first;            
        }
        
        // do not forget the head key (if it is set and not _KEYMAX)
        if (_headKey != _KEYMAX) {
            if (_headKey > _ramMaximum) _ramMaximum = _headKey;
            if (_headKey < _ramMinimum) _ramMinimum = _headKey;        
        }
    }

};

#endif
