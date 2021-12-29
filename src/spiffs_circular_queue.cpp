/**
* @file spiffs_circular_queue.cpp
* SPIFFS Circular Queue (aka FIFO) implementation file.
* @author rykovv
**/

#include "spiffs_circular_queue.h"
#include "sys/stat.h"

#define SPIFFS_CIRCULAR_QUEUE_HEAD_FRONT    (0)
#define SPIFFS_CIRCULAR_QUEUE_HEAD_BACK     (sizeof(uint16_t))

/// private function to mount SPIFFS during initialization
uint8_t _mount_spiffs(void);
/// private function to unmount SPIFFS when you don't need it, i.e. before going in a sleep mode
void    _unmount_spiffs(void);

uint8_t spiffs_circular_queue_init(circular_queue_t *cq, const uint8_t mount_spiffs = 1) {
    uint8_t ret = 1;

    if (mount_spiffs && !_mount_spiffs()) ret = 0;

    if (ret) {
        struct stat sb;

        // stat returns 0 upon succes (file exists) and -1 on failure (does not)
        if (stat(cq->fn, &sb) < 0) {
            if ((cq->fd = fopen(cq->fn, "w"))) {
                fclose(cq->fd);
            } else {
                ret = 0;
            }
        }
    }

    return ret;
}

uint8_t spiffs_circular_queue_front(uint8_t * elem) {
    uint8_t ret = 0;

    if (!spiffs_circular_queue_is_empty()) {
        if ((_fd = fopen(send_queue_filename, "r+b"))) {
            fseek(_fd, _front * SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE, SEEK_SET);

            ret = SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE == fread(elem, 1, SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE, _fd);

            fclose(_fd);
        }
    }

    return ret;
}

uint8_t spiffs_circular_queue_enqueue(uint8_t * elem) {
    uint8_t ret = 0;

    if (spiffs_circular_queue_size() < SPIFFS_CIRCULAR_QUEUE_MAX_SIZE) {
        if ((_fd = fopen(send_queue_filename, "r+b"))) {
            fseek(_fd, _back * SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE, SEEK_SET);

            size_t nwritten;
            if ((ret = (SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE == (nwritten = fwrite(elem, 1, SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE, _fd))))) {
                _back = (++_back) % SPIFFS_CIRCULAR_QUEUE_MAX_SIZE;
                _size++;
            }

            fclose(_fd);
        }
    }

    return ret;
}

void spiffs_circular_queue_dequeue() {
    if (!spiffs_circular_queue_is_empty()) {
        _front = (++_front) % SPIFFS_CIRCULAR_QUEUE_MAX_SIZE;
        _size--;
    }
}

uint8_t spiffs_circular_queue_is_empty() {
    return !_size && (_front == _back);
}

uint16_t spiffs_circular_queue_size(circular_queue_t *cq) {
    return (cq->back >= cq->front? (cq->back - cq->front) : (SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - cq->front + cq->back));
}

uint16_t spiffs_circular_queue_get_front_ptr() {
    return _front;
}

uint16_t spiffs_circular_queue_get_back_ptr() {
    return _back;
}

uint8_t spiffs_circular_queue_free(void) {
    uint8_t ret = 0;

    if (!remove(send_queue_filename)) {
        _unmount_spiffs();

        ret = 1;
    }

    return ret;
}

uint8_t _mount_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 2,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    return (ret == ESP_OK);
}

void _unmount_spiffs(void) {
    esp_vfs_spiffs_unregister(NULL);
}
