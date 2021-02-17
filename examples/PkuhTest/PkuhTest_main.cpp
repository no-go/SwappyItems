#include <cstdio>      // printf
#include <cstdlib>     // srand, rand, exit
#include <inttypes.h>  // uintX_t stuff
#include <tuple>
#include "../../src/SwappyQueue.hpp"

using namespace std;

#define ITEMLIMIT 16384

/** @example PkuhTest_main.cpp
 * This is a netbeans8.2 (2018 build) example, how to use the priority queue in the Pkuh class.
 * In the future I will do Dijkstra Stuff with it. */

typedef SwappyQueue<uint64_t, double> SwKuh;

int main(int argc, char** argv) {
    SwKuh * pq;
    pq = new SwKuh();
    bool exi;
    uint64_t key = 0;
    
    for (uint64_t i = 0; i < ITEMLIMIT; ++i) {
        int d = 1;
        d = rand()%50;
        uint64_t q = rand()%ITEMLIMIT;
        
        SwKuh::Element e;
        e.data = 0.01 * rand();
        e.prio = 5 + rand()%1000;
        if (pq->set(q, e) == false) {
            printf("Update %6" PRIu64 "\n", q);
        } else {
            ++key;
            printf("Insert %6" PRIu64 " (size %" PRIu64 ")\n", q, key);
        }
        
        if (d == 0) {
            pq->del(q);
            printf(" Delete %6" PRIu64 " (size %" PRIu64 ")\n", q, --key);
        }
    }

    printf("\nrun pop() now!\n\n");
    for (uint64_t i = 0; i < ITEMLIMIT; ++i) {
        SwKuh::Element e;
        exi = pq->top(key, e);
        if (exi == false) {
            i = ITEMLIMIT;
        } else {
            printf("%5lu key: %" PRIu64 " prio: %" PRIu64 " data: %lf\n", i+1, key, e.prio, e.data);
            pq->del(key);
        }
    }
    
    delete(pq);
    return 0;
}