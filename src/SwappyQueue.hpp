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
        Heap::Data * p;
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
        Heap::Data * p = _data->get(key);
        
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
        Heap::statistic_s s = _data->getStatistic();
        if (s.size == 0) return;
        
        if (s.size == 1) {
            if (key == _headKey) {
                _data->del(__headKey);
                empty = true;
            }
            return;
        }
        
        if (s.size > 1) {

            Heap::Data * itemPair = _data->get(key); // = remember the datas from old element
            
            if (itemPair == nullptr) return;

            Item item = itemPair->first;
            std::vector<TKEY> siblings(itemPair->second);
            
            _data->del(key);

            std::vector<TKEY> newsiblings;
            std::vector<TKEY> winners;
            TKEY champion;

            if (key != _headKey) {
                Heap::Data * parentPair = _data->get(item.parent);
                // remove key from siblings of parent
                for (auto n : parentPair->second) {
                    if (n != key) newsiblings.push_back(n);
                }
                _data->set(item.parent, parentPair->first, newsiblings);
            }

            /// @todo recode to swappyItems!!   -------------------------------------------v
    
            // build pairs (1. phase)
            for (uint64_t i=0; i < siblings.size(); i+=2) {
                if ((i+1) >= siblings.size()) {
                    // a single element ...
                    winners.push_back(siblings[i]);
                } else {
                    if (_data[siblings[i]].prio < _data[siblings[i+1]].prio) {
                        winners.push_back(siblings[i]);
                        _data[siblings[i+1]].parent = siblings[i];
                        _data[siblings[i]].siblings.push_back(siblings[i+1]);
                    } else {
                        winners.push_back(siblings[i+1]);
                        _data[siblings[i]].parent = siblings[i+1];
                        _data[siblings[i+1]].siblings.push_back(siblings[i]);
                    }
                }
            }
            // build a single tree (2. phase)
            if (winners.size() > 0) {
                champion = winners[0];
                for (uint64_t i=1; i < winners.size(); ++i) {
                    if (_data[winners[i]].prio < _data[champion].prio) {
                        _data[winners[i]].siblings.push_back(champion);
                        _data[champion].parent = winners[i];
                        champion = winners[i];
                    } else {
                        _data[champion].siblings.push_back(winners[i]);
                        _data[winners[i]].parent = champion;
                    }
                }

                // may set a new head
                if (key == _headKey) {
                    _headKey               = champion; // the winner of all winners
                    _data[_headKey].parent = _headKey; // no parent = link to its self!
                } else {
                    _data[champion].parent = element.parent;
                    _data[element.parent].siblings.push_back(champion);
                }
            }
            
            /// @todo recode to swappyItems!!   -------------------------------------------A
        }
    }
    
private:
    
    // key should not exist!!!
    /// @todo force a 2 pahse, if head siblings are to many (e.g. EACHFILE/2)
    void insert (TKEY key, Item & item) {
        Heap::statistic_s s = _data->getStatistic();
        if (s.size == 0) {
            _headKey = key;
            item.parent = key;
            _data->set(key, item, std::vector<TKEY>(0));
            empty = false;            
        } else {
            Heap::Data * headpair = _data->get(_headKey);
            
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