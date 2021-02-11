#include <cstdio>      // printf
#include <cstdlib>     // srand, rand, exit
#include <inttypes.h>  // uintX_t stuff
#include <tuple>
#include "../../src/Pkuh.hpp"

using namespace std;

#define ITEMLIMIT 65536

typedef Pkuh<
        uint64_t,
        double, 
        (ITEMLIMIT/2), 
        8, 
        3, 
        4, 
        2*4*(ITEMLIMIT/2)
> PriQueue;

/** @example PkuhTest_main.cpp
 * This is a netbeans8.2 (2018 build) example, how to use the priority queue in the Pkuh class.
 * In the future I will do Dijkstra Stuff with it. */
 
int main(int argc, char** argv) {
    PriQueue * pq;
    PriQueue::statistic_s st;
    pq = new PriQueue(23);
    bool exi;
    uint64_t key = 0;
    
    for (uint64_t i = 0; i < ITEMLIMIT; ++i) {
        int d = rand()%5;
        uint64_t q = rand()%ITEMLIMIT;
        if (pq->set(q, 0.01 * rand()) == false) {
            printf("u");//printf("Update %5lu\n", q);
        } else {
            ++key;
            printf("i");//printf("Insert %5lu (size %lu)\n", q, key);
        }
        pq->update(q, 5 + rand()%1000);
        
        if (d == 0) {
            pq->del(q);
            printf(" Delete %5lu (size %lu)\n", q, --key);
        }
        if (0 == (i%1000)) {
            st = pq->getStatistic();
            
            printf("\nsize %5lu\n",st.size);
        }
    }

    printf("\nrun pop() now!\n\n");
    for (uint64_t i = 0; i < ITEMLIMIT; ++i) {
        PriQueue::Data e;
        exi = pq->pop(key, e);
        if (exi == false) {
            i = ITEMLIMIT;
        } else {
            printf("%5lu key: %5lu prio: %3lu data: %lf\n", i+1, key, std::get<2>(e), std::get<0>(e));
        }
    }
    
    delete(pq);
    return 0;
}