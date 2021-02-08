#include <cstdio>      // printf
#include <cstdlib>     // srand, rand, exit
#include <inttypes.h>  // uintX_t stuff
#include <tuple>
#include "../../src/Pkuh.hpp"

using namespace std;

/** @example main.cpp
 * This is a netbeans8.2 (2018 build) example, how to use the priority queue in the PKuh class.
 * In the future I will do Dijkstra Stuff with it. */
 
int main(int argc, char** argv) {
    Pkuh<uint64_t, double> * pq = new Pkuh<uint64_t, double>(23);
    bool exi;
    uint64_t key = 0;
    
    for (uint64_t i = 0; i < 500000; ++i) {
        uint64_t r = rand()%500000;
        if (pq->set(i, 0.01 * rand()) == false) {
            //printf("Update %5ld\n", i);
        } else {
            //printf("Insert %5ld (size %ld)\n", i, ++key);
        }
        pq->update(i, 5 + rand()%1000);
        
        if (pq->del(r)) ;//printf(" Delete %5ld (size %ld)\n", r, --key);
    }

    printf("\nrun pop() now!\n\n");
    for (uint64_t i = 0; i < 500000; ++i) {
        Pkuh<uint64_t, double>::Data e;
        exi = pq->pop(key, e);
        if (exi == false) {
            i = 500000;
        } else {
            printf("%5ld key: %5ld prio: %3ld data: %lf\n", i+1, key, std::get<2>(e), std::get<0>(e));
            if (std::get<2>(e) == 0) {
                printf("zero prio should not happend!\n");
            }
        }
    }
    
    // result: after 60min and 12 files, pop makes "30378 key:  2570 prio:   5 data: 8897924.210000" same lines and is killed!!!
    
    delete(pq);
    return 0;
}