#ifndef _BUFFER_HPP
#define _BUFFER_HPP 1
#include <mutex>
#include <deque>
#include "Semaphore.hpp"
using namespace std;

/**
 * Das ist ein Bounded-Buffer. Sollte die Semaphore mit ->P() auf
 * die Zahl 0 herunter gezählt worden sein, blockiert die Methode pop().
 * In dem Fall sind in der "Queue" auch keine Elemente, die entfernt
 * werden können. Beim push() wird mit ->V() die Semaphore wieder
 * raufgezählt, da zuvor ein Element in die Queue abgelegt wurde.
 * Weil mehrere Threads gleichzeitig zugriff auf den Buffer haben,
 * könnte pop() und push() zeitgleich stattfinden. Um beim Lesen und
 * Scheiben auf den Queue keine Probleme zu bekommen (Stichwort: thread-safe)
 * ist dieser Zugriff bzw. der kritische Abschnitt mit einer
 * Mutex-Semaphore gesichert.
 */
template<typename ELEMENTTYPE>
class Buffer
{
private:
	deque<ELEMENTTYPE> _values;
	Semaphore * _filled;
	mutex _mu;

public:
	Buffer()
	{
		_filled = new Semaphore(0);
	}

	virtual ~Buffer()
	{
		delete _filled;
	}

	ELEMENTTYPE pop()
	{
		_filled->P(); // blockieren, wenn leer
		_mu.lock(); // start: kritischer Abschnitt -------.
			ELEMENTTYPE _return;                   //     |
			_return = _values.back();              //     |
			_values.pop_back();                    //     |
		_mu.unlock(); // ende: kritischer Abschnitt ------'
		return _return;
	}

	void push(ELEMENTTYPE ele)
	{
		_mu.lock(); // start: kritischer Abschnitt ------.
			_values.push_back(ele);               //     |
		_mu.unlock(); // ende: kritischer Abschnitt -----'
		_filled->V(); // freigeben
	}

};
#endif