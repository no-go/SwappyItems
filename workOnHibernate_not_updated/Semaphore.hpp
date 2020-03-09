#ifndef _SEMAPHORE_HPP
#define _SEMAPHORE_HPP 1
#include <mutex>
#include <atomic>
#include <condition_variable>
class Semaphore {
private:
    std::mutex _mtx;
    std::condition_variable _cv;
    std::atomic<int> _count;
public:
    Semaphore(int count = 0) {
        _count = count;
    }
    // Vreigeben
    void V() {
        std::unique_lock<std::mutex> lck(_mtx);
        _count++;
        _cv.notify_one();
    }
    // Parken, falls nicht freigegeben
    void P() {
        std::unique_lock<std::mutex> lck(_mtx);
        while(_count == 0) _cv.wait(lck);
        _count--;
    }
};
#endif

