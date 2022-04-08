#include <Arduino.h>
#include "spiffs_circular_queue.h"

circular_queue_t cq;

void make_data(void);
void send_data(void);

void setup() {
    // load cq from EEPROM or other external source
    // ..
    snprintf(cq.fn, SPIFFS_FILE_NAME_MAX_SIZE, "spiffs/data");
    spiffs_circular_queue_init(&cq);
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
    uint8_t buf[SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE];
    // ...
    // read data from sensors, generate network packets, etc
    // ...
    spiffs_circular_queue_enqueue(&cq, buf, SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE);
}

void send_data(void) {
    uint8_t buf[SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE];
    uint32_t buf_size;
    // ...
    // prepare everything for sending data
    // ...
    if (!spiffs_circular_queue_is_empty(&cq)) {
        while (spiffs_circular_queue_size(&cq) > 0) {
            spiffs_circular_queue_front(&cq, buf, &buf_size);
            // send buf and pop the queue
            spiffs_circular_queue_dequeue(&cq);
        }
    }
}
