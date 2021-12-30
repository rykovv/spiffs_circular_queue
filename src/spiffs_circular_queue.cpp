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
uint8_t _write_medium(const circular_queue_t *cq, const void *data, const uint32_t data_size);
uint8_t _read_medium(const circular_queue_t *cq, void *data, uint32_t *data_size, const uint8_t size_only = 0);

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

uint8_t spiffs_circular_queue_front(const circular_queue_t *cq, uint8_t * elem, uint32_t *elem_size) {
    uint8_t ret = 0;

    if (!spiffs_circular_queue_is_empty()) {
        ret = _read_medium(cq, elem, elem_size);
    }

    return ret;
}

uint8_t spiffs_circular_queue_enqueue(circular_queue_t *cq, const uint8_t * elem, const uint32_t elem_size) {
    uint8_t ret = 0;

    if (spiffs_circular_queue_size(cq) + elem_size <= SPIFFS_CIRCULAR_QUEUE_MAX_SIZE) {
        if (_write_medium(cq, elem, elem_size)) {
            cq->back = (cq->back + sizeof(uint32_t) + elem_size) % SPIFFS_CIRCULAR_QUEUE_MAX_SIZE;
            ret = 1;
        }
    }

    return ret;
}

uint8_t spiffs_circular_queue_dequeue(circular_queue_t *cq) {
    uint8_t ret = 0;

    if (!spiffs_circular_queue_is_empty(cq)) {
        // read last elem size
        uint32_t back_elem_size = 0;
        if (_read_medium(cq, NULL, &back_elem_size, 1)) {
            // advance front index
            cq->front = (cq->front + sizeof(uint32_t) + back_elem_size) % SPIFFS_CIRCULAR_QUEUE_MAX_SIZE;
            ret = 1;
        }
    }

    return ret;
}

uint_8_t spiffs_circular_queue_persist(const circular_queue_t *cq) {
    FILE *fd = NULL;
    uint8_t nwritten = 0;

    if ((fd = fopen(cq->fn, "r+b"))) {
        // write front and back indices to the file's head
        nwritten += fwrite(&(cq->front), 1, sizeof(cq->front), fd);
        nwritten += fwrite(&(cq->back), 1, sizeof(cq->back), fd);
    
        fclose(fd);
    }
    
    return (nwritten == SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET);
}

uint8_t spiffs_circular_queue_is_empty(const circular_queue_t *cq) {
    return !spiffs_circular_queue_size(cq);
}

uint16_t spiffs_circular_queue_size(const circular_queue_t *cq) {
    return (cq->back >= cq->front? (cq->back - cq->front) : (SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - cq->front + cq->back));
}

uint16_t spiffs_circular_queue_get_front_idx(const circular_queue_t *cq) {
    return cq->front;
}

uint16_t spiffs_circular_queue_get_back_idx(const circular_queue_t *cq) {
    return cq->back;
}

uint8_t spiffs_circular_queue_free(const circular_queue_t *cq, const uint8_t unmount_spiffs = 1) {
    uint8_t ret = 0;

    if (!remove(cq->fn)) {
        if (unmount_spiffs) _unmount_spiffs();
        memset(cq, 0x0, sizeof(circular_queue_t));
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

uint8_t _write_medium(const circular_queue_t *cq, const void *data, const uint32_t data_size) {
    // spiffs medium
    FILE *fd = NULL;
    uint32_t nwritten = 0;

    if ((fd = fopen(cq->fn, "r+b"))) {
        fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back, SEEK_SET);

        // case 1: splitted elem size
        if (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back + sizeof(uint32_t) > SPIFFS_CIRCULAR_QUEUE_MAX_SIZE) {
            // TODO: test!!
            uint8_t buf[sizeof(uint32_t)];
            // transform data_size into byte array
            memcpy(buf, &data_size, sizeof(uint32_t));
            // write first part of data_size
            nwritten = fwrite(buf, 1, SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back), fd);
            // set seek to the first usable byte
            fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET, SEEK_SET);
            // write the rest of data_size
            nwritten += fwrite(&buf[SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back)], 1, 
                sizeof(uint32_t) - SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back), fd);
        } else { // normal write
            nwritten = fwrite(&data_size, 1, sizeof(uint32_t), fd);
        }

        // case 2: splitted elem data
        if (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front + data_size > SPIFFS_CIRCULAR_QUEUE_MAX_SIZE) {
            nwritten += fwrite(data, 1, SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back), fd);
            fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET, SEEK_SET);
            nwritten += fwrite(&data[SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back)], 1, 
                data_size - SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back), fd);
        } else { // normal write
            nwritten += fwrite(data, 1, data_size, fd);
        }

        fclose(fd);
    }

    return (nwritten == (sizeof(uint32_t) + data_size));
}

// data_size must not be null
uint8_t _read_medium(const circular_queue_t *cq, void *data, uint32_t *data_size, const uint8_t size_only = 0) {
    // spiffs medium
    FILE *fd = NULL;
    uint32_t nread = 0;

    if ((fd = fopen(cq->fn, "r+b"))) {
        fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front, SEEK_SET);

        // case 1: splitted elem size
        if (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front + sizeof(uint32_t) > SPIFFS_CIRCULAR_QUEUE_MAX_SIZE) {
            // TODO: test!!
            uint8_t buf[sizeof(uint32_t)];
            // read first half
            nread = fread(buf, 1, SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front), fd);
            // set seek to the first usable byte
            fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET, SEEK_SET);
            // read the rest of size
            nread += fread(&buf[SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front)], 1, 
                sizeof(uint32_t) - SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front), fd);
            // transform read by bytes data_size into uint32_t
            memcpy(data_size, buf, sizeof(data_size));
        } else { // normal read
            nread = fread(data_size, 1, sizeof(uint32_t), fd);
        }

        if (!size_only) {
            // case 2: splitted elem data
            if (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front + *data_size > SPIFFS_CIRCULAR_QUEUE_MAX_SIZE) {
                nread += fread(data, 1, SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front), fd);
                // set seek to the first usable byte
                fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET, SEEK_SET);
                // read the rest of data
                nread += fread(&data[SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front)], 1, 
                    *data_size - SPIFFS_CIRCULAR_QUEUE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front), fd);
            } else { // normal read
                nread += fread(data, 1, *data_size, fd);
            }
        }

        fclose(fd);
    }

    return (nread == (size_only? sizeof(uint32_t) : (sizeof(uint32_t) + *data_size)));
}