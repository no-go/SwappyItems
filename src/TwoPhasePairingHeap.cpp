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
        if (_data.size() > 0) {
            /// @todo was, wenn es der head ist!!!!??
            
            
            
            
            
            
            Element elm; // = remember the datas from old element
            if (get(key, elm)) {
                _data.erase(key);
                
                vector<uint64_t> tomerge;
                vector<uint64_t> winners;
                
                // remove key from siblings of parent
                for (auto n : _data[elm.parent].siblings) {
                    if (n != key) tomerge.push_back(n);
                }
                _data[elm.parent].siblings = tomerge;

                // collect siblings from removed element
                for (auto n : elm.siblings) {
                    // assing to a valid parent
                    n.parent = elm.parent;
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
                uint64_t champion = winners[0];
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
        }
        if (_data.size() == 0) empty = true;
    }
    
    /**
     * @return true, if it is new and not just an update
     */
    bool set (uint64_t key, Element element) {
        try {
            _data.at(key);
            if (_data[key].prio == element.prio) {
                _data[key] = element;
            } else {
                /// @todo still exist and the prio changed!
                
                // key is the head
                // yes
                    // new prio is smaller?
                    // yes = fine. nothing to do                
                    // no = .......

                // key is the head
                // no
                    // prio is lower than head?
                    // yes = it is the new head!
                    
                    // prio is lower than head?
                    // no = ....
                
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
