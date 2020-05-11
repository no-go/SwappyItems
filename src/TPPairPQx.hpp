/** Two Pass Pairing based Priority Queue eXtended (prio 1 = high)
 * 
 * - change priority is possible
 * 
 * Extended:
 * 
 * - cut of elements with low priority
 * - direct key access to remove or get copy of element
 */

template <class DATAVALUE>
class TPPairPQx {

public:

    typedef struct Element_s {
        uint64_t  key;
        uint64_t  prio;
        DATAVALUE data;
    } Element;

    uint64_t size();
    
    bool push(Element element);
    
    /**
     * get copy of element [with highest prio]
     */
    Element get();
    Element get(uint64_t key);
    
    /** 
     * get copy of element [with highest prio] and removes it
     */
    Element pop();
    Element pop(key);
    
    bool set_priority(uint64_t key, uint64_t prio);
    
    /**
     * erases all elements behind the first "num" elements with highest 
     * prio and returns it as a new TwoPPPQ
     */
    TwoPPPQ cut(uint64_t num);

private:

    void merge();
    void twoPass();
};