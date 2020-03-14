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
- flat: no need of a fat framework
- no additional libs!

Go to the [Documentation of SwappyItems](https://no-go.github.io/SwappyItems/)
or take a look to the [SwappyItems Source Code](https://github.com/no-go/SwappyItems/).

## API basics

### constructor(id)

It trys to wakeup from hibernate files. The id identifies the SwappyItems store.

### destructor

Makes a hibernate from ram to files.

### `set(key, value)`

Set or create an item. It returns `false`, if the item is created (and not overwritten).

### `set(key, value, vector<keys>)`

Set or create an item. It returns `false`, if the item is created (and not overwritten).
Your can add a vector with additional keys in it.

### `pair(value, vector<keys>) = get(key)`

Get the values by key. Your get a pointer to that tuple. If it does not exist, you get a `nullptr`.

### `del(key)`

Delete a item.

### `getStatistic()`

You get a struct with many values (inserts, sizes, deletes, lookup failures, ...) to
deploy the SwappyItems Store.

### `each()`

- iterate through all elements at a fixed state
- with a lambda function it is similar to a "find"
- can change data via lambda
- gives a reference back to last processed data

### `apply()`

It is usable with a lambda function and similar to `get()` and `set()`.
It does not change the swap data and behavior and thus this access it a bit slow.
If you want to get or change data in the same SwappyItems store, where you
iter through with `each()`, then you must use `apply()` in the lambda
function of `each()`.
