# The SwappyItems Key-Values Store

The SwappyItems Store is a flexible c++17 multicore single disk key-value storage.
If the data in RAM gets large, it behaves similar to your system swap, but
with more parameters and it swaps only the data - not the program code!

Main difference to other key-value stores:

- Buckets with Bloom-filter instead of B*-tree or Log-structured merge-tree (LSM)
- posibility of persistance and breakpoints
- not RAM only, not DISK only
- very flexible in data: value is not just "String"
- not focused on multible disks or networking
- flat: no need of a fat framework (just include `SwappyItems.hpp`)
- no additional libs!
- not all keys are stored in RAM: SwappyItems uses min/max and bloomfilter to search keys in files
- modern c++17 build-in multicore usage
- modern c++11 lambda expression API

Go to the [Documentation of SwappyItems](https://no-go.github.io/SwappyItems/)
or take a look to the [SwappyItems Source Code](https://github.com/no-go/SwappyItems/).

## Examples

Only the examples (see `Makefile`) need `-lz -losmpbf -lprotobuf` (additional libs) for phrasing OSM data.

## ToDo (nice to have)

- implement a larger code version with PriorityQueue (for a minimal spanning
  tree use-case)
- make number of items in swapped files variable with additional focus on real
  RAM usage, and ...
- Because the `vector<key>` data can be very big, SwappyItems should additionally
  swap data from RAM to DISK, if to many items *and* vector elements are in RAM.
- Implement a bucket based Priority Queue or building parallel processing winner trees in PriorityQueue heap.
  Idea for buckets: onion-like with two SwappyItems stores. Use `key` as
  a `prio` bucket and `vector<key>` with all keys with same `prio`. These keys are the keys from the
  2nd SwappyItems Store, where the original data items is stored.
- split writing into files (a swap event makes some sorted file buckets) in multiple threads
- do prefetching in PriorityQueue for *next to actual min prio* Items
- minimal spanning tree example code

### SwappyQueue

- I try to implement a 2 phase pairing heap on the head of SwappyItems.
- testing with netbeans8.2 dev (2018) in `examples/SwappyQueueTest/` folder
- still with bugs :-(

## SwappyItems: API basics

### (constructor)

It trys to SwappyItems::wakeup() from hibernate files. The id parameter identifies 
the SwappyItems Store (important for the DISK/swap path!).

### (destructor)

Makes a SwappyItems::hibernate() from RAM to DISK.

### bool = set(key, value)

Set or create an item. It returns `false`, if the item is created (and not overwritten).

### bool = set(key, value, vector<keys>)

Set or create an item. It returns `false`, if the item is created (and not overwritten).
You can add a vector with additional keys in it.

### pair(value, vector<keys>) * = get(key)

Get the value by key. You get a pointer to that data pair. If it does not exist, you get a `nullptr`.

### del(key)

Delete a item.

### min() and max()

Get the smallest or the biggest key in the SwappyItems Store.

### getStatistic()

You get a SwappyItems::statistic_s with many values (inserts, sizes, deletes, lookup failures, ...) to
deploy the SwappyItems Store.

### bool = each(back, lambda func)

- iterate through all elements at a fixed state
- with a lambda function it is similar to a "find" (if lambda returns `true`, the `each()` breaks)
- can change data via lambda
- gives a reference back to last processed data
- returns `false` if the loop did not break and iterated through all items

### bool = apply(back, key, lambda func)

It is usable with a lambda function and similar to SwappyItems::get() and SwappyItems::set().
It does not change the swap data and behavior and thus this access it a bit slow.
If you want to get or change data in the same SwappyItems store, where you
iter through with SwappyItems::each(), then you must use SwappyItems::apply() in the lambda
function of SwappyItems::each(). It returns `false` if the item does not exit.

