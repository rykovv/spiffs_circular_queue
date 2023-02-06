# SPIFFS Circular Queue

SPIFFS Circular Queue or Circular FIFO Buffer is a data structure similar to a regular array queue in which the last data byte is connected to the first data byte. The queue metadata is maintained through circular_queue_t struct in RAM and are kept up to date in queue header in SPIFFS. Refer to circular_queue_t definition to see precisely what is stored in RAM. On the other hand, special effort was put to optimize non-volatile (nv) footprint. 

Queue max size is settable for each queue instance through max_size member of circular_queue_t. Elems can be of variable or fixed size. If elems are of variable size (elem_size = 0), the elem size is stored at the beginning of each elem. If elem size if fixed (elem_size > 0), no elem size is stored.

Elems size of a variable elem size queue can also be limited, if you find it useful to keep it under control. Set SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE to a max size in bytes you may want, and when you want to enqueue an elem size that exceeds the SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE value it will be truncated. If you want to keep it unconstraint, just keep it on zero.

Basic nv structure is split in two parts: metadata header and data body. The header is composed of a fixed (always-stored) and optional parts. Optional part is stored iff optional feature(s) is/are enabled. Metadata header is depicted below.

```
┌─────────────┬────────────┬───────────┬───────────┬──────────┬─ ─ ─ ─ ─ ─┐
│ front index | back index |   count   | max size  |   flags  | elem size |
|  (4 bytes)  | (4 bytes)  | (2 bytes) | (4 bytes) | (1 byte) | (2 bytes) |
└─────────────┴────────────┴───────────┴───────────┴──────────┴─ ─ ─ ─ ─ ─┘
 \_____________________________._____________________________/ \____.____/
                           fixed part                          optional part
```
The flags is a one-byte bitfield with the following information
```
┌─────────────────┬──────────┬────────────┐
│ fixed elem size | reserved | queue type |
|     (1 bit)     | (3 bits) |  (4 bits)  |
└─────────────────┴──────────┴────────────┘
```
Data body can follow two patterns depending if the queue elem size is variable or fixed.
```
┌─────────────┬────────┬───────┬─────────────┬────────┐    ┌────────┬───────┬────────┐
│ 1 elem size | 1 elem |  ...  | n elem size | n elem |    │ 1 elem |  ...  | n elem |
|  (4 bytes)  |  data  |       |  (2 bytes)  |  data  |    |  data  |       |  data  |
└─────────────┴────────┴───────┴─────────────┴────────┘    └────────┴───────┴────────┘
 \_________________________._________________________/      \___________.___________/
             variable elem size data body                   fixed elem size data body 
```

Fixed metadata header is used to keep track of queue pointers and allows to fully restore the queue after a power loss or (un)expected reset. Nodes may vary in size what comes very handy when you need to store network packets or any data of variable size. If your data of fixed size like timestamps or sensor readings, this project is a good fit for you as well.

It was carefully unit-tested on ESP32 considering important general and corner cases. To see testes cases refer to unit_testing/test_main.ino. 


## Interface

### spiffs_circular_queue_init

Initializes the library creating/reading a spiffs data file. Sets current front, back, and count queue indices. For non-volatile storages the initialization and re-initialization depend on the written queue data. The library will set its indices and count variables to what is encounted on the medium or set to zeros if found none. If you modify cq struct variables outside of the library, that will be lost if _persist function will not be called. Initialization will result in failure only on null cq struct pointer, failure to mount SPIFFS, or failure to write queue data file on SPIFFS.
```cpp
uint8_t spiffs_circular_queue_init(circular_queue_t *cq);
```
Returns 1 on success and 0 on fail.

### spiffs_circular_queue_front

Places front queue elem of elem_size size to the elem.
```cpp
uint8_t spiffs_circular_queue_front(const circular_queue_t *cq, void *elem = NULL, uint16_t *elem_size = NULL);
cq->front(const circular_queue_t *cq, void *elem, uint16_t *elem_size); // variable elem size
cq->front(const circular_queue_t *cq, void *elem, NULL /* don't care */); // fixed elem size
```
Returns 1 on success and 0 on fail.

### spiffs_circular_queue_enqueue

Enqueues elem of elem_size size to the front of the queue if there is enough room in the queue. Be responsible for passing elem buffer of SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE size or less if SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE is enabled.
```cpp
uint8_t spiffs_circular_queue_enqueue(circular_queue_t *cq, const void *elem = NULL, const uint16_t elem_size = 0);
cq->enqueue(circular_queue_t *cq, const void *elem, const uint16_t elem_size); // variable elem size
cq->enqueue(circular_queue_t *cq, const void *elem, 0 /* don't care */); // fixed elem size
```
Returns 1 on success and 0 on fail.

### spiffs_circular_queue_dequeue

Pops out the first elem of the queue. When elem and elem_size are valid pointers, front elem is placed in them and then it pops out.
```cpp
uint8_t spiffs_circular_queue_dequeue(circular_queue_t *cq, void *elem = NULL, uint16_t *elem_size = NULL);
cq->dequeue(circular_queue_t *cq, void *elem, uint16_t elem_size); // variable elem size
cq->dequeue(circular_queue_t *cq, void *elem, NULL /* don't care */); // fixed elem size
```
Returns 1 on success and 0 on fail.

### spiffs_circular_queue_is_empty

Checks whether the queue is empty or not.
```cpp
uint8_t spiffs_circular_queue_is_empty(const circular_queue_t *cq);
cq->is_empty(const circular_queue_t *cq);
```
Returns 1 when empty and 0 if not.

### spiffs_circular_queue_size

Returns the current queue net data size. This function is useful when you need to know how much pure data you still need to process/send. It does not account nodes sizes stored at head of each node.
```cpp
uint32_t spiffs_circular_queue_size(const circular_queue_t *cq);
cq->size(const circular_queue_t *cq);
```
Returns the current queue net data size.

### spiffs_circular_queue_available_space

Returns queue available space in bytes. Allows to return a real estimate for the next elem to be enqueued. Caution: not upper limited by the SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE. Check SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE value to see max elem size you can enqueue if enabled.
```cpp
uint32_t spiffs_circular_queue_available_space(const circular_queue_t *cq);
cq->available_space(const circular_queue_t *cq);
```
Returns queue available space in bytes.

### spiffs_circular_queue_get_front_idx

Gets the front index of the queue.
```cpp
uint32_t spiffs_circular_queue_get_front_idx(const circular_queue_t *cq);
cq->get_front_idx(const circular_queue_t *cq);
```
Returns front index in bytes.

### spiffs_circular_queue_get_back_idx

Gets the back index of the queue.
```cpp
uint32_t spiffs_circular_queue_get_back_idx(const circular_queue_t *cq);
cq->get_back_idx(const circular_queue_t *cq);
```
Returns back index in bytes.

### spiffs_circular_queue_get_count

Gets the queue nodes count.
```cpp
uint16_t spiffs_circular_queue_get_count(const circular_queue_t *cq);
cq->get_count(const circular_queue_t *cq);
```
Returns queue nodes count.

### spiffs_circular_queue_get_file_size

Gets the queue SPIFFS file footprint in bytes.
```cpp
uint32_t spiffs_circular_queue_get_file_size(const circular_queue_t *cq);
cq->get_file_size(const circular_queue_t *cq);
```
Returns queue SPIFFS file size in bytes.

### spiffs_circular_queue_free

Frees resourses allocated for the queue and closes the SPIFFS.
```cpp
uint8_t	spiffs_circular_queue_free(circular_queue_t *cq, const uint8_t unmount_spiffs = 1);
cq->free(circular_queue_t *cq, const uint8_t unmount_spiffs = 1);
```
Returns 1 on success and 0 on fail.

## Typical example

Multiple instances of different queues can peacefully coexist. This is a typical example. 

```cpp
#include <Arduino.h>
#include "spiffs_circular_queue.h"

circular_queue_t cq_variable;   // variable elem size
circular_queue_t cq_fixed;      // fixed elem size

void setup() {
    // Very important point to get right filename
    //   queues are distinguished by filenames. 
    //   Don't forget '/spiffs/' prefix!
    snprintf(cq_variable.fn, SPIFFS_FILE_NAME_MAX_SIZE, "/spiffs/variable");
    cq_variable.max_size = 2048;
    spiffs_circular_queue_init(&cq_variable);

    snprintf(cq_fixed.fn, SPIFFS_FILE_NAME_MAX_SIZE, "/spiffs/variable");
    cq_fixed.elem_size = sizeof(uint64_t);
    cq_fixed.max_size = sizeof(uint64_t)*100;
    spiffs_circular_queue_init(&cq_fixed);
}

void loop() {
    uint8_t buf[128] = {1, 2, 3};
    uint16_t buf_len = 3;

    uint64_t elem = 4223;

    spiffs_circular_queue_enqueue(&cq_variable, buf, buf_len);
    spiffs_circular_queue_enqueue(&cq_fixed, &elem);
    
    //...
    // If necessary go to sleep. All data are safe in SPIFFS     
    //...

    // Flush the queue
    spiffs_circular_queue_foreach_dequeue(&cq_variable, buf, &buf_len) {
        // process each buf in the queue
    }

    spiffs_circular_queue_foreach_dequeue(&cq_fixed, &elem) {
        // process each elem in the queue
    }

    // Go to sleep again if necessary
    
    // If you need to remove queue from the spiffs for whatever reason...
    spiffs_circular_queue_free(&cq_variable);
    spiffs_circular_queue_free(&cq_fixed);
    // BE CAREFULL, you will lost all your enqueued data if some, and
    //  the circular_queue_t struct will be zeroed.
}
```

## SPIFFS Consideration

The library will reliably work as far as you keep the used (or intended to be used) space under 75% of assigned to SPIFFS partition.

## Attribution

If this project gets someone's appreciation, I want to transfer this appreciation to my God, Lord Jesus Christ.

## License

The source code for the project is licensed under the MIT license, which you can find in the LICENSE.txt file.
