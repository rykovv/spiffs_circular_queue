/**
* @file spiffs_circular_queue.cpp
* SPIFFS Circular Queue (aka FIFO) implementation file.
* @author rykovv
**/

#include "spiffs_circular_queue.h"
#include "sys/stat.h"

#define SPIFFS_CIRCULAR_QUEUE_HEAD_FRONT    (0)
#define SPIFFS_CIRCULAR_QUEUE_HEAD_BACK     (sizeof(uint32_t))
#define SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET   (sizeof(uint32_t)*2)

/// private function to mount SPIFFS during initialization
uint8_t  _mount_spiffs(void);
/// private function to unmount SPIFFS when you don't need it, i.e. before going in a sleep mode
void     _unmount_spiffs(void);
uint32_t _write_medium(circular_queue_t *cq, void *data, uint32_t data_size);
uint32_t _read_medium(circular_queue_t *cq, void *data, uint32_t *data_size);

uint8_t spiffs_circular_queue_init(circular_queue_t *cq, const uint8_t mount_spiffs = 1) {
    uint8_t ret = 1;

    if (mount_spiffs && !_mount_spiffs()) ret = 0;

    if (ret) {
        struct stat sb;
        FILE *fd = NULL;

        // stat returns 0 upon succes (file exists) and -1 on failure (does not)
        if (stat(cq->fn, &sb) < 0) {
            if ((fd = fopen(cq->fn, "w"))) {
                cq->front = 0;
                cq->back = 0;
                
                // set front and back indices in the file's head
                uint8_t nwritten = fwrite(&(cq->front), 1, sizeof(cq->front), fd);
                nwritten += fwrite(&(cq->back), 1, sizeof(cq->back), fd);
                if (nwritten != SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET) ret = 0;
                
                fclose(fd);
            } else {
                ret = 0;
            }
        } else {
            if ((fd = fopen(cq->fn, "r+b"))) {
                // read front and back indices from the file's head
                uint8_t nread = fread(&(cq->front), 1, sizeof(cq->front), fd);
                nread += fread(&(cq->back), 1, sizeof(cq->back), fd);
                if (nread != SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET) ret = 0;

                fclose(fd);
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

uint8_t spiffs_circular_queue_is_empty(circular_queue_t *cq) {
    return !spiffs_circular_queue_size(cq) && (cq->front == cq->back);
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

uint32_t _write_medium(circular_queue_t *cq, void *data, uint32_t data_size) {
    // spiffs medium
    
}

// data_size must not be null
uint32_t _read_medium(circular_queue_t *cq, void *data, uint32_t *data_size) {
    // spiffs medium
    FILE *fd = NULL;
    uint32_t nread = 0;

    if ((fd = fopen(cq->fn, "r+b"))) {
        fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front, SEEK_SET);

        // case 1: splitted elem size
        if (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front + sizeof(uint32_t) > SPIFFS_CIRCULAR_QUEUE_MAX_SIZE) {
            // TODO: test!!
            uint8_t buf[sizeof(uint32_t)];
            // for little endian
            // read first half
            fread(buf, 1, SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front), fd);
            // set seek to the first usable byte
            fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET, SEEK_SET);
            // read the rest of size
            fread(&buf[SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front)], 1, 
                sizeof(uint32_t) - SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front), fd);
            // transform read by bytes data_size into uint32_t
            memcpy(data_size, buf, sizeof(data_size));
        } else { // normal read
            nread = fread(data_size, 1, sizeof(uint32_t), fd);
        }

        // case 2: splitted elem data

        ret = SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE == fread(elem, 1, SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE, _fd);

        fclose(_fd);
    }
}