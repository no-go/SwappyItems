#ifndef __SWAPPY_QUEUE_HPP__
#define __SWAPPY_QUEUE_HPP__ 1

#include <vector>
#include <inttypes.h>  // uintX_t stuff
#include "SwappyItems.hpp"

template <
    class TKEY,
    class TVALUE,
        
    int EACHFILE = 16384,
    int OLDIES = 8,
    int RAMSIZE = 3,
    int BLOOMBITS = 4,
    int MASKLENGTH = (2* 4*16384)
>
class SwappyQueue {

public:
    bool empty;

    struct Item {
        TVALUE      data;
        uint64_t    prio;
        TKEY      parent; // @todo programmer: please do not touch!?!
    };

private:
    
    typedef SwappyItems<TKEY, Item, EACHFILE, OLDIES, RAMSIZE, BLOOMBITS, MASKLENGTH> Heap;
    
    Heap * _data;
    TKEY _headKey;    

public:
    
    SwappyQueue (void) {
        empty = true;
        _data = new Heap(42);
    }
    
    ~SwappyQueue (void) {
        delete(_data);
    }
    
    /**
     * @return false, if it does not exist
     */
    bool get (TKEY key, Item & result) {
        typename Heap::Data * p;
        p = _data->get(key);
        if (p == nullptr) return false;
        result = p->first;
        return true;
    }
    
    /**
     * @return false, if it does not exist
     */
    bool top (TKEY & resultkey, Item & result) {
        if (empty) {
            return false;
        } else {
            resultkey = _headKey;
            return get(_headKey, result);
        }
    }
   
    /**
     * @return true, if it is new and not just an update
     */
    bool set (TKEY key, Item & item) {
        typename Heap::Data * p = _data->get(key);
        
        if (p == nullptr) {
            insert(key, item);
            return true;            
        } else {
            if (p->first.prio == item.prio) {
                _data->set(key, item);
            } else {
                del(key);
                insert(key, item);
            }
            return false;       
        }
    }
    
    void del (TKEY key) {
        typename Heap::statistic_s s = _data->getStatistic();
        if (s.size == 0) return;
        
        if (s.size == 1) {
            if (key == _headKey) {
                _data->del(_headKey);
                empty = true;
            }
            return;
        }
        
        if (s.size > 1) {
            typename Heap::Data * dummy1Pair;
            typename Heap::Data * dummy2Pair = _data->get(key); 
            
            if (dummy2Pair == nullptr) return;

            // = remember the datas from old element
            Item item = dummy2Pair->first;
            std::vector<TKEY> siblings(dummy2Pair->second);
            
            _data->del(key);

            std::vector<TKEY> newsiblings;
            std::vector<TKEY> winners;
            TKEY champion;

            if (key != _headKey) {
                dummy1Pair = _data->get(item.parent);
                // remove key from siblings of parent
                for (auto n : dummy1Pair->second) {
                    if (n != key) newsiblings.push_back(n);
                }
                _data->set(item.parent, dummy1Pair->first, newsiblings);
            }

            // build pairs (1. phase)
            for (uint64_t i=0; i < siblings.size(); i+=2) {
                if ((i+1) >= siblings.size()) {
                    // a single element ...
                    winners.push_back(siblings[i]);
                } else {
                    dummy1Pair = _data->get(siblings[i]);
                    dummy2Pair = _data->get(siblings[i+1]);
                    if (dummy1Pair->first.prio < dummy2Pair->first.prio) {
                        winners.push_back(siblings[i]);
                        dummy2Pair->first.parent = siblings[i];
                        dummy1Pair->second.push_back(siblings[i+1]);
                        //_data->set(...); // hey, we use pointers. the data in _ramList should be accessable and changed!
                    } else {
                        winners.push_back(siblings[i+1]);
                        dummy1Pair->first.parent = siblings[i+1];
                        dummy2Pair->second.push_back(siblings[i]);
                        //_data->set(...); // hey, we use pointers. the data in _ramList should be accessable and changed!
                    }
                }
            }
    
            // build a single tree (2. phase)
            if (winners.size() > 0) {
                champion = winners[0];
                dummy1Pair = _data->get(champion);
                
                for (uint64_t i=1; i < winners.size(); ++i) {
                    dummy2Pair = _data->get(winners[i]);
                    if (dummy2Pair->first.prio < dummy1Pair->first.prio) {
                        // we found a new head/champion
                        dummy2Pair->second.push_back(champion);
                        dummy1Pair->first.parent = winners[i];
                        champion = winners[i];
                        dummy1Pair = dummy2Pair;
                    } else {
                        dummy1Pair->second.push_back(winners[i]);
                        dummy2Pair->first.parent = champion;
                    }
                }

                // may set a new head
                if (key == _headKey) {
                    _headKey = champion;                 // the winner of all winners
                    dummy1Pair = _data->get(_headKey);
                    dummy1Pair->first.parent = _headKey; // no parent = link to its self!
                } else {
                    dummy1Pair = _data->get(champion);
                    dummy1Pair->first.parent = item.parent;
                    dummy2Pair = _data->get(item.parent);
                    dummy2Pair->second.push_back(champion);
                }
            }

        }
    }
    
private:
    
    // key should not exist!!!
    /// @todo force a 2 pahse, if head siblings are to many (e.g. EACHFILE/2)
    void insert (TKEY key, Item & item) {
        typename Heap::statistic_s s = _data->getStatistic();
        if (s.size == 0) {
            _headKey = key;
            item.parent = key;
            _data->set(key, item, std::vector<TKEY>(0));
            empty = false;            
        } else {
            typename Heap::Data * headpair = _data->get(_headKey);
            
            if (item.prio < headpair->first.prio) {
                item.parent = key;
                std::vector<TKEY> siblings;
                siblings.push_back(_headKey);
                _data->set(key, item, siblings);
                
                headpair->first.parent = key;
                _data->set(_headKey, headpair->first);
                
                _headKey = key;
            } else {
                item.parent = _headKey;
                _data->set(key, item, std::vector<TKEY>(0));
                
                headpair->second.push_back(key);
                _data->set(_headKey, headpair->first, headpair->second);
            }
        }
    }
};

#endif 