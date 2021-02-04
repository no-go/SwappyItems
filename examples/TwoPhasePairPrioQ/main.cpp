#include <cstdio>      // printf
#include <cstdlib>     // srand, rand, exit
#include <inttypes.h>  // uintX_t stuff
#include "TwoPhasePairingHeap.hpp"

int main(int argc, char** argv) {
    TwoPhasePairingHeap<uint64_t> pq;
    bool exi;
    uint64_t k;
    
    for (int i = 0; i < 200; ++i) {
        TwoPhasePairingHeap<uint64_t>::Element e;
        e.data = rand();
        e.prio = 5 + rand()%10;
        pq.set(rand()%200, e);
        pq.del(rand()%200);
    }
    for (int i = 0; i < 20; ++i) {
        TwoPhasePairingHeap<uint64_t>::Element e;
        exi = pq.get(i, e);
        if (exi) printf("%5d prio: %3d data: %12d\n", i, e.prio, e.data);
    }
    for (int i = 0; i < 20; ++i) {
        TwoPhasePairingHeap<uint64_t>::Element e;
        exi = pq.top(k, e);
        if (exi == false) {
            i = 200;
        } else {
            printf("%5d %12d %3d\n", i, k, e.prio);
            if (e.prio == 0) printf("zero prio should not happend!\n");
            pq.del(k);
        }
    }
    return 0;
}


