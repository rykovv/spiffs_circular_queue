#include <Arduino.h>
#include "spiffs_circular_queue.h"

struct _cqueue {
    uint16_t send_queue_front = 0;
    uint16_t send_queue_back  = 0;
} cqueue_t;

cqueue_t cq;

void make_data(void);
void send_data(void);

void setup() {
    // load cq from EEPROM or other external source
    // ..
    spiffs_circular_queue_init(cq.send_queue_front, cq.send_queue_back);
}

void loop() {
    make_data();
    //...
    // If necessary go to sleep. Don't forget to save and restore cq indices data
    //   using spiffs_circular_queue_get_front_indx and spiffs_circular_queue_get_back_indx.
    //   Finally, spiffs_circular_queue_free
    // once woke up, run spiffs_circular_queue_init again
    //...
    send_data();
    // go to sleep again if necessary
}

void make_data(void) {
    uint8_t buf[SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE];
    // ...
    // read data from sensors, generate network packets, etc
    // ...
    spiffs_circular_queue_enqueue(buf);
}

void send_data(void) {
    uint8_t buf[SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE];
    // ...
    // prepare everything for sending data
    // ...
    if (!spiffs_circular_queue_is_empty()) {
        while (spiffs_circular_queue_size() > 0) {
            spiffs_circular_queue_front(buf);
            // send buf and pop the queue
            spiffs_circular_queue_dequeue();
        }
    }
}
