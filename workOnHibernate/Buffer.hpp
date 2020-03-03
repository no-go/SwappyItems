#ifndef _BUFFER_HPP
#define _BUFFER_HPP 1
#include <vector>
#include <mutex>
#include <deque>
#include <string>
#include "JobTool.hpp"
#include "Semaphore.hpp"
using namespace std;

template<typename ELEMENTTYPE>
class Buffer {
private:
	vector<deque<ELEMENTTYPE> > _values;
	int _deepness;
	Semaphore * _filled;
	mutex _mu;

public:
	Buffer(int layers) {
		_filled = new Semaphore(0);
		_deepness = layers+1;
		_values.resize(_deepness);
	}

	virtual ~Buffer() {
		delete _filled;
	}

	ELEMENTTYPE pop(const int & limit) {
		_filled->P();
		_mu.lock();
		ELEMENTTYPE _return;
		bool found = false;
		for (int i = _deepness; i > limit; --i) {
			if (_values[i-1].size() != 0) {
				// -> depth-first search
				_return = _values[i-1].back();
				_values[i-1].pop_back();
				found = true;
				break;
			}
		}
		if (found == false) {
			for (int i = 0; i < limit; i++) {
				if (_values[i].size() != 0) {
					// -> breadth-first search
					_return = _values[i].front();
					_values[i].pop_front();
					break;
				}
			}
		}
		_mu.unlock();

		return _return;
	}

	void push(ELEMENTTYPE job, const int & K, const int endK) {
		vector<ELEMENTTYPE> jobs = JobTool::branch(job, K, endK);
		for (auto it = jobs.begin(); it != jobs.end(); ++it) {
			_mu.lock();
			_values[JobTool::deepness(*it)].push_back(*it);
			_mu.unlock();
			_filled->V();
		}
	}

};
#endif