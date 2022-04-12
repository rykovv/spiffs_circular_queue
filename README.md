# SPIFFS Circular Queue

SPIFFS Circular Queue or Circular FIFO Buffer is a regular queue in which the last element is connected to the first element. It is a dynamic structure with initially dynamic file layout. Once it has grown to SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE the file footprint is kept constant while the data varies. Its basic structure is shown below.

```
┌─────────────┬────────────┬───────────┬─────────────┬────────┬───────┬─────────────┬────────┐
│ front index | back index |   count   | 1 elem size | 1 elem |  ...  | n elem size | n elem |
|  (4 bytes)  | (4 bytes)  | (2 bytes) |  (2 bytes)  |  data  |       |  (2 bytes)  |  data  |
└─────────────┴────────────┴───────────┴─────────────┴────────┴───────┴─────────────┴────────┘
 \__________________._________________/ \_________________________._________________________/
               fixed header                                 variable body 
```

Fixed header is used to keep track of queue pointers and especially useful when the system goes down or RAM content is lost. Nodes vary in size what comes very handy when you need to store network packets or any data of variable size.

It was carefully tested on ESP32 considering important general and corner cases. Such cases as split elem size or split elem data due to circular nature were working very fine. 

Multiple instances of different queues can peacefully coexist. This is a typical example. 

```cpp
#include <Arduino.h>
#include "spiffs_circular_queue.h"

circular_queue_t cq;

void setup() {
    // Very important point to get right filename
    //   queues are distinguished by filenames. 
    //   Don't forget '/spiffs/' prefix!
    snprintf(cq.fn, SPIFFS_FILE_NAME_MAX_SIZE, "/spiffs/send_data");
    if (spiffs_circular_queue_init(&cq)) {
        printf("spiffs_circular_queue_init [SUCCESS]\n");
    }
}

void loop() {
    uint8_t buf[SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE];
    uint16_t buf_len = 0;

    // Read data from sensors, generate network packets, etc.
    make_data(buf, &buf_len);

    // Here you are responsible for passing a buffer with
    //  enough space to store the next elem in it.
    if (spiffs_circular_queue_enqueue(&cq, buf, buf_len)) {
        printf("spiffs_circular_queue_enqueue [SUCCESS]\n");
    }

    //...
    // If necessary go to sleep. All data are safe in SPIFFS     
    //...

    // Flush the queue
    while (!spiffs_circular_queue_is_empty(&cq)) {
        // If you need to just to get data without popping it out
        //   use this.
        if (spiffs_circular_queue_front(&cq, buf, &buf_size)) {
            printf("spiffs_circular_queue_front [SUCCESS]\n");
        }
        // Do some stuff with data (i.e. send) and then pop out.
        if (spiffs_circular_queue_dequeue(&cq)) {
            printf("spiffs_circular_queue_dequeue [SUCCESS]\n");
        }

        // Alternatively you can use the overloaded instance of 
        //   spiffs_circular_queue_dequeue function that returns you
        //   next queue node, and then pops it out.

        // if (spiffs_circular_queue_dequeue(&cq, buf, &buf_size)) {
        //     printf("spiffs_circular_queue_dequeue [SUCCESS]\n");
        // }
    }

    // Go to sleep again if necessary
    
    // If you need to remove queue from the spiffs for whatever reason...
    if (spiffs_circular_queue_free(&cq)) {
        printf("spiffs_circular_queue_free [SUCCESS]\n");
    }
    // BE CAREFULL, you will lost all your enqueued data if some, and
    //  the circular_queue_t struct will be zeroed.
}
```

## SPIFFS Consideration

The library will reliably work as far as you keep the used (or intended to be used) space under 75% of assigned to SPIFFS partition.

## License

The source code for the project is licensed under the MIT license, which you can find in the LICENSE.txt file.
