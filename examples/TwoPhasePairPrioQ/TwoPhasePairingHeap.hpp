#ifndef TWOPHASEPAIRINGHEAP_HPP
#define TWOPHASEPAIRINGHEAP_HPP

#include <vector>
#include <inttypes.h>  // uintX_t stuff
#include <unordered_map>
#include <stdexcept>   // std::out_of_range

using namespace std;

template <class TVALUE>
class TwoPhasePairingHeap {
    
public:

    struct Element {
        TVALUE               data;
        uint64_t             prio;
    private:
        uint64_t           parent;
        vector<uint64_t> siblings;
        friend class TwoPhasePairingHeap;
    };
    
    bool empty;
    
    TwoPhasePairingHeap (void) {
        empty = true;
    }
    
    ~TwoPhasePairingHeap (void) {}
    
    /**
     * @return false, if it does not exist
     */
    bool get (uint64_t key, Element & result) {
        try {
            _data.at(key);
            result = _data[key];
            return true;
        } catch (const out_of_range & oor) {
            return false;
        }
    }
    
    /**
     * @return false, if it does not exist
     */
    bool top (uint64_t & resultkey, Element & result) {
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
    bool set (uint64_t key, Element & element) {
        try {
            _data.at(key);
            if (_data[key].prio == element.prio) {
                _data[key] = element;
            } else {
                del(key);
                insert(key, element);
            }
            return false;
            
        } catch (const out_of_range & oor) {            
            insert(key, element);
            return true;
        }
    }
    
    void del (uint64_t key) {
        if (_data.size() == 1) {
            
            if (key == _headKey) _data.erase(key);
            
        } else if (_data.size() > 1) {

            Element element; // = remember the datas from old element
            if (get(key, element)) {
                _data.erase(key);
                
                vector<uint64_t> newsiblings;
                vector<uint64_t> winners;
                uint64_t champion;
                
                if (key != _headKey) {
                    // remove key from siblings of parent
                    for (auto n : _data[element.parent].siblings) {
                        if (n != key) newsiblings.push_back(n);
                    }
                    _data[element.parent].siblings = newsiblings;
                }
                                
                // build pairs (1. phase)
                for (int i=0; i < element.siblings.size(); i+=2) {
                    if ((i+1) >= element.siblings.size()) {
                        // a single element ...
                        winners.push_back(element.siblings[i]);
                    } else {
                        if (_data[element.siblings[i]].prio < _data[element.siblings[i+1]].prio) {
                            winners.push_back(element.siblings[i]);
                            _data[element.siblings[i+1]].parent = element.siblings[i];
                            _data[element.siblings[i]].siblings.push_back(element.siblings[i+1]);
                        } else {
                            winners.push_back(element.siblings[i+1]);
                            _data[element.siblings[i]].parent = element.siblings[i+1];
                            _data[element.siblings[i+1]].siblings.push_back(element.siblings[i]);
                        }
                    }
                }
                // build a single tree (2. phase)
                if (winners.size() > 0) {
                    champion = winners[0];
                    for (int i=1; i < winners.size(); ++i) {
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
            }
        }
        if (_data.size() == 0) empty = true;
    }
    
private:
    
    unordered_map<uint64_t, Element> _data;
    uint64_t _headKey;
    
    // key should not exist!!!
    void insert (uint64_t key, Element & element) {
        if (_data.size() == 0) {
            _headKey = key;
            _data[key] = element;
            _data[key].parent = key;
            empty = false;            
        } else {
            _data[key] = element;
            if (_data[key].prio < _data[_headKey].prio) {
                _data[key].parent = key;
                _data[key].siblings.push_back(_headKey);
                _data[_headKey].parent = key;
                _headKey = key;
            } else {
                _data[key].parent = _headKey;
                _data[_headKey].siblings.push_back(key);                
            }
        }
    }
};

#endif /* TWOPHASEPAIRINGHEAP_HPP */

