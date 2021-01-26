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
    
    void merge () {
        
    }
    
    void mergePair () {
        
    }
    
public:
    
    TwoPhasePairingHeap (void) {}
    
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
        return get(_headKey, result);
    }
    
    void del (uint64_t key) {
        _data.erase(key);
    }
    
    /**
     * @return true, if it is new and not just an update
     */
    bool set (uint64_t key, Element element) {
        try {
            _data.at(key);
            _data[key] = element;
            element.parent = 88;
            return false;
        } catch (const out_of_range & oor) {
            _data[key] = element;
            return true;
        }
    }
};

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
