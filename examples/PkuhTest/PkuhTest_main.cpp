#include <cstdio>      // printf
#include <cstdlib>     // srand, rand, exit
#include <inttypes.h>  // uintX_t stuff
#include <tuple>
#include "../../src/Pkuh.hpp"

using namespace std;

#define ITEMLIMIT 16384

typedef Pkuh<
        uint64_t,
        double, 
        128, // items each file
        4, // files each swap 
        3, // size of RAM until full and do a swap
        4, 
        2* 4* 128
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
    
    //uint64_t keys[] = {6, 63, 36, 49};
    //uint64_t prios[] = {10, 8, 6, 8};
    
    for (uint64_t i = 0; i < ITEMLIMIT; ++i) {
        int d = 1;
        //d = rand()%50;
        uint64_t q = rand()%ITEMLIMIT;
        //uint64_t q = keys[i];
        
        if (pq->set(q, 0.01 * rand()) == false) {
            printf("Update %6" PRIu64 "\n", q);
        } else {
            ++key;
            printf("Insert %6" PRIu64 " (size %" PRIu64 ")\n", q, key);
        }
        pq->update(q, 5 + rand()%1000);
        //pq->update(q, prios[i]);
        
        if (d == 0) {
            pq->del(q);
            printf(" Delete %6" PRIu64 " (size %" PRIu64 ")\n", q, --key);
        }
    }

    printf("\nrun pop() now!\n\n");
    for (uint64_t i = 0; i < ITEMLIMIT; ++i) {
        PriQueue::Data e;
        exi = pq->pop(key, e);
        if (exi == false) {
            i = ITEMLIMIT;
        } else {
            printf("%5lu key: %" PRIu64 " prio: %" PRIu64 " data: %lf\n", i+1, key, std::get<2>(e), std::get<0>(e));
        }
    }
    
    /// @todo print heap and file content!
    
    delete(pq);
    return 0;
}