#include <Arduino.h>
#include "spiffs_circular_queue.h"

circular_queue_t cq;

// dumb function to make data
void make_seq (uint8_t n, uint8_t *arr, uint8_t arr_size);
void make_data(uint16_t n);
void send_data(void);

void setup() {
    // very important point to get right filename
    //   queues are distinguished by filenames
    //   Don't forget '/spiffs/' prefix!
    snprintf(cq.fn, SPIFFS_FILE_NAME_MAX_SIZE, "/spiffs/send_data");
    if (spiffs_circular_queue_init(&cq)) {
        printf("spiffs_circular_queue_init [SUCCESS]\n");
    }
}

void loop() {
    make_data(123);
    //...
    // If necessary go to sleep. All data are safe in SPIFFS     
    //...
    send_data();
    // go to sleep again if necessary
    
    // if you need to remove queue from the spiffs for whatever reason
    //  then use
    if (spiffs_circular_queue_free(&cq)) {
        printf("spiffs_circular_queue_free [SUCCESS]\n");
    }
    // BE CAREFULL, you will lost all your enqueued data if some, and
    //  the circular_queue_t struct will be zeroed.
}

void make_data(uint16_t n) {
    // ...
    // read data from sensors, generate network packets, etc
    // ...
    // make some data
    uint8_t buf[SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE];
    uint16_t buf_len = n;
    make_seq(n, buf, SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE);
    
    // here you are responsible for passing a buffer with
    //  enough space to store the next elem in it.
    if (spiffs_circular_queue_enqueue(&cq, buf, buf_len)) {
        printf("spiffs_circular_queue_enqueue [SUCCESS]\n");
    }
}

void send_data(void) {
    uint8_t buf[SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE];
    uint16_t buf_size = 0;
    // ...
    // prepare everything for sending data
    // ...
    while (!spiffs_circular_queue_is_empty(&cq)) {
        // if you need to just to get data without popping it out
        //   use this
        if (spiffs_circular_queue_front(&cq, buf, &buf_size)) {
            printf("spiffs_circular_queue_front [SUCCESS]\n");
        }
        // do some stuff with data (i.e. send) and then pop out
        if (spiffs_circular_queue_dequeue(&cq)) {
            printf("spiffs_circular_queue_dequeue [SUCCESS]\n");
        }

        // alternatively you can use the overloaded instance of 
        //   spiffs_circular_queue_dequeue function that returns you
        //   next queue node, and then pops it out

        // if (spiffs_circular_queue_dequeue(&cq, buf, &buf_size)) {
        //     printf("spiffs_circular_queue_dequeue [SUCCESS]\n");
        // }
    }
}

void make_seq (uint8_t n, uint8_t *arr, uint8_t arr_size) {
    for (uint8_t i = 0; i < n && i < arr_size; i++) {
        arr[i] = i;
    }
}