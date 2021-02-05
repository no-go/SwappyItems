#include <cstdio>      // printf
#include <cstdlib>     // srand, rand, exit
#include <inttypes.h>  // uintX_t stuff
#include "TwoPhasePairingHeap.hpp"

int main(int argc, char** argv) {
    TwoPhasePairingHeap<uint64_t> pq;
    bool exi;
    uint64_t key;
    
    for (uint64_t i = 0; i < 2000; ++i) {
        TwoPhasePairingHeap<uint64_t>::Element e;
        e.data = rand();
        e.prio = 5 + rand()%10;
        uint64_t q = rand()%2000;
        uint64_t r = rand()%2000;
        if (pq.set(q, e) == false) {
            printf("Update %5d\n", q);
        } else {
            printf("Insert %5d\n", q);            
        }
        pq.del(r);
        printf("Delete %5d\n", r);
    }
//    for (uint64_t i = 0; i < 2000; ++i) {
//        TwoPhasePairingHeap<uint64_t>::Element e;
//        exi = pq.get(i, e);
//        if (exi) {
//            if (e.prio == 0) {
//                printf("%5d prio: %3d data: %12d\n", i, e.prio, e.data);
//                printf("zero prio should not happend!\n");
//            }
//            //pq.del(i);
//        }
//    }

    printf("\nrun top() now!\n\n");
    for (uint64_t i = 0; i < 2000; ++i) {
        TwoPhasePairingHeap<uint64_t>::Element e;
        exi = pq.top(key, e);
        if (exi == false) {
            i = 2000;
        } else {
            printf("%5d prio: %3d data: %12d\n", key, e.prio, e.data);
            if (e.prio == 0) {
                printf("zero prio should not happend!\n");
            }
            pq.del(key);
        }
    }
    return 0;
}

