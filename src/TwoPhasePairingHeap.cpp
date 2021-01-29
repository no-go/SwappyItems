#include <vector>
#include <cstdio>
#include <inttypes.h>  // uintX_t stuff
#include <unordered_map>
#include <stdexcept>   // std::out_of_range
#include <cstdlib>     // srand, rand, exit

// g++ TwoPhasePairingHeap.cpp -Wall

using namespace std;

struct Element {
public:
    int                  data;
    uint64_t             prio;
private:
    uint64_t           parent;
    vector<uint64_t> siblings;
    friend class TwoPhasePairingHeap;
};

class TwoPhasePairingHeap {
private:
    unordered_map<uint64_t, Element> _data;
    uint64_t _headKey;
    
public:

    bool empty;
    
    TwoPhasePairingHeap (void) {
        empty = true;
    }
    
    ~TwoPhasePairingHeap (void) {}
    
    /**
     * @return false, if it does not exist
     */
    bool get(uint64_t key, Element & result) {
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
    bool top(Element & result) {
        if (empty) {
            return false;
        } else {
            return get(_headKey, result);
        }
    }
    
    void del (uint64_t key) {
        if (_data.size() == 1) {
            
            if (key == _headKey) _data.erase(key);
            
        } else if (_data.size() > 1) {

            Element elm; // = remember the datas from old element
            if (get(key, elm)) {
                _data.erase(key);
                
                vector<uint64_t> tomerge;
                vector<uint64_t> winners;
                uint64_t champion;
                
                if (key != _headKey) {
                    // remove key from siblings of parent
                    for (auto n : _data[elm.parent].siblings) {
                        if (n != key) tomerge.push_back(n);
                    }
                    _data[elm.parent].siblings = tomerge;
                }

                // collect siblings from removed element
                for (auto n : elm.siblings) {
                    // assing to a valid parent
                    if (key != _headKey) {
                        _data[n].parent = elm.parent;
                    } else {
                        _data[n].parent = n;
                    }
                    tomerge.push_back(n);
                }
                // tomerge has the keys from all elements, where parent key
                // may or must change!
                
                // build pairs (1. phase)
                for (int i=0; i < tomerge.size(); i+=2) {
                    if ((i+1) >= tomerge.size()) {
                        // a single element ...
                        winners.push_back(tomerge[i]);
                    } else {
                        if (_data[tomerge[i]].prio < _data[tomerge[i+1]].prio) {
                            winners.push_back(tomerge[i]);
                            _data[tomerge[i+1]].parent = tomerge[i];
                            _data[tomerge[i]].siblings.push_back(tomerge[i+1]);
                        } else {
                            winners.push_back(tomerge[i+1]);
                            _data[tomerge[i]].parent = tomerge[i+1];
                            _data[tomerge[i+1]].siblings.push_back(tomerge[i]);
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
                            _data[champion].siblings.push_back(tomerge[i]);
                            _data[winners[i]].parent = champion;
                        }
                    }
                    
                    // may set a new head
                    if (key == _headKey) {
                        _headKey               = champion; // the winner of all winners
                        _data[_headKey].parent = _headKey; // no parent = link to its self!
                    }
                }
            }
        }
        if (_data.size() == 0) empty = true;
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
                /// still exist and the prio changed!
                
                if (key == _headKey) {
                    // key is the head -----------------------------
                    
                    // new prio is bigger?
                    if (_data[key].prio < element.prio) {
                        _data[key] = element;
                        // prio of siblings may be smaller, now!
                        
                        vector<uint64_t> tomerge;
                        vector<uint64_t> winners;
                        uint64_t champion = _headKey;
                        
                        tomerge.push_back(key);
                        
                        // make siblings to possible heads
                        for (auto n : _data[key].siblings) {
                            _data[n].parent = n;
                            tomerge.push_back(n);
                        }
                        
                        // phase 1
                        for (int i=0; i < tomerge.size(); i+=2) {
                            if ((i+1) >= tomerge.size()) {
                                // a single element ...
                                winners.push_back(tomerge[i]);
                            } else {
                                if (_data[tomerge[i]].prio < _data[tomerge[i+1]].prio) {
                                    winners.push_back(tomerge[i]);
                                    _data[tomerge[i+1]].parent = tomerge[i];
                                    _data[tomerge[i]].siblings.push_back(tomerge[i+1]);
                                } else {
                                    winners.push_back(tomerge[i+1]);
                                    _data[tomerge[i]].parent = tomerge[i+1];
                                    _data[tomerge[i+1]].siblings.push_back(tomerge[i]);
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
                                    _data[champion].siblings.push_back(tomerge[i]);
                                    _data[winners[i]].parent = champion;
                                }
                            }
                        }
                        _headKey               = champion; // the winner of all winners
                        _data[_headKey].parent = _headKey; // no parent = link to its self!

                    } else {
                        // no = fine. nothing to do. min gets more minimal!
                        _data[key] = element;
                    }
                    
                } else {
                    // key is NOT the head -------------------------

                    // prio is lower than head -> element will be the new head!
                    if (_data[_headKey].prio > element.prio) {
                        
                        // remove key from siblings of parent
                        vector<uint64_t> newsib;
                        for (auto n : _data[element.parent].siblings) {
                            if (n != key) newsib.push_back(n);
                        }
                        _data[element.parent].siblings = newsib;
                        _data[_headKey].parent         = key;    // old head gets a parent!
                        element.parent                 = key;    // no parent = link to its self!
                        _data[key].siblings.push_back(_headKey); // next new head gets a sibling (the old parent)
                        _headKey   = key;
                        _data[key] = element;
                        
                    } else {
                        // new prio is NOT lower than head
                        
                        // new prio is bigger than before?
                        if (_data[key].prio < element.prio) {
                            _data[key] = element;
                            /// @todo prio of siblings may be smaller, now!
                            
                            
                            
                            
                            
                            
                            
                            
                            
                            
                            
                            

                        } else {
                            if (_data[element.parent].prio > _data[key].prio) {
                                _data[key] = element;
                                /// @todo no = NOT fine! something to do
                                
                                
                                
                                
                                
                                
                                
                                
                                
                                
                            } else {
                                // is parent still smaller? -> nothing to do
                                _data[key] = element;
                            }
                        }
                    }
                }
                
            }
            return false;
            
        } catch (const out_of_range & oor) {
            
            _data[key]          = element;
            _data[key].siblings = vector<uint64_t>(0);
            if (empty) {
                _headKey          = key;        // new element is new head
                _data[key].parent = _headKey;   // no parent = link to its self!
            } else {
                // it is new and the queue is not empty!
                if (_data[key].prio < _data[_headKey].prio) {
                    _data[_headKey].parent = key;            // old head gets a parent!
                    _data[key].siblings.push_back(_headKey); // next new head gets a sibling (the old parent)
                    _headKey          = key;                 // new element is new head
                    _data[key].parent = _headKey;            // no parent = link to its self!
                } else {
                    _data[_headKey].siblings.push_back(key); // head gets a new sibling
                    _data[key].parent   = _headKey;          // head is the parent
                }
            }
            empty = false;
            return true;
        }
    }
};

// ----------------------------------------------------------------------------------------

int main(void) {
    TwoPhasePairingHeap pq;
    
    for (int i = 0; i < 20000; ++i) {
        Element e;
        e.data = rand();
        e.prio = 5 + rand()%50;
        pq.set(rand()%20000, e);
        pq.del(rand()%20000);
    }
    for (int i = 0; i < 20000; ++i) {
        Element e;
        bool exi = pq.get(rand()%20000, e);
        if (exi) printf("%5d Data: %d\n", i, e.data);
    }
    return 0;
}
