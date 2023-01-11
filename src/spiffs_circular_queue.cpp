/**
* @file spiffs_circular_queue.cpp
* SPIFFS Circular Queue (aka FIFO) implementation file.
* @author rykovv
**/

#include "spiffs_circular_queue.h"

#ifdef ESP32
#include "sys/stat.h"
#include "esp_spiffs.h"
#else
#error Library designed to work with ESP32-IDE and x-tensa toolchain 
#endif

#define SPIFFS_CIRCULAR_QUEUE_HEAD_FRONT    (0)                     ///< Front index file offset
#define SPIFFS_CIRCULAR_QUEUE_HEAD_BACK     (sizeof(uint32_t))      ///< Back index file offset
#define SPIFFS_CIRCULAR_QUEUE_HEAD_COUNT    (sizeof(uint16_t))      ///< Elements count file offset
#define SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET   (sizeof(uint32_t)*2 + \
                                             sizeof(uint16_t))      ///< Data location file offset
#define SPIFFS_CIRCULAR_QUEUE_MAX_DATA_SIZE (SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - \
                                            SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET)  ///< Max data size

/// private function to mount SPIFFS during initialization
static uint8_t _mount_spiffs(void);
/// private function to unmount SPIFFS when you don't need it, i.e. before going in a sleep mode
static uint8_t _unmount_spiffs(void);
/// private function that adds write medium-independent abstraction
static uint8_t _write_medium(const circular_queue_t *cq, const uint8_t *data, const uint16_t data_size);
/// private function that adds read medium-independent abstraction. data = NULL to read only the size of last elem
static uint8_t _read_medium(const circular_queue_t *cq, uint8_t *data, uint16_t *data_size);
/// private function that saves current pointers to the queue file
static uint8_t _spiffs_circular_queue_persist(const circular_queue_t *cq);

uint8_t spiffs_circular_queue_init(circular_queue_t *cq) {
    uint8_t ret = 1;

    if (!esp_spiffs_mounted(NULL)) {
        ret = _mount_spiffs();
    }

    if (ret) {
        struct stat sb;
        FILE *fd = NULL;

        // stat returns 0 upon succes (file exists) and -1 on failure (does not)
        if (stat(cq->fn, &sb) < 0) {
            if ((fd = fopen(cq->fn, "w"))) {
                cq->front_idx = 0;
                cq->back_idx = 0;
                cq->count = 0;
                
                // set front and back indices in the file's head
                uint8_t nwritten = fwrite(&(cq->front_idx), 1, sizeof(cq->front_idx), fd);
                nwritten += fwrite(&(cq->back_idx), 1, sizeof(cq->back_idx), fd);
                nwritten += fwrite(&(cq->count), 1, sizeof(cq->count), fd);
                if (nwritten != SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET) ret = 0;
                
                fclose(fd);
            } else {
                ret = 0;
            }
        } else {
            if ((fd = fopen(cq->fn, "r+b"))) {
                // read front and back indices from the file's head
                uint8_t nread = fread(&(cq->front_idx), 1, sizeof(cq->front_idx), fd);
                nread += fread(&(cq->back_idx), 1, sizeof(cq->back_idx), fd);
                nread += fread(&(cq->count), 1, sizeof(cq->count), fd);
                if (nread != SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET) ret = 0;

                fclose(fd);
            } else {
                ret = 0;
            }
        }
    }

    if (ret) {
        cq->front = spiffs_circular_queue_front;
        cq->enqueue = spiffs_circular_queue_enqueue;
        cq->dequeue = spiffs_circular_queue_dequeue;
        cq->is_empty = spiffs_circular_queue_is_empty;
        cq->size = spiffs_circular_queue_size;
        cq->available_space = spiffs_circular_queue_available_space;
        cq->get_front_idx = spiffs_circular_queue_get_front_idx;
        cq->get_back_idx = spiffs_circular_queue_get_back_idx;
        cq->get_count = spiffs_circular_queue_get_count;
        cq->get_file_size = spiffs_circular_queue_get_file_size;
        cq->free = spiffs_circular_queue_free;
    }

    return ret;
}

// Be responsible for passing elem of SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE
uint8_t spiffs_circular_queue_front(const circular_queue_t *cq, uint8_t *elem, uint16_t *elem_size) {
    uint8_t ret = 0;

    if (!spiffs_circular_queue_is_empty(cq)) {
        ret = _read_medium(cq, elem, elem_size);
    }

    return ret;
}

uint8_t spiffs_circular_queue_enqueue(circular_queue_t *cq, const uint8_t * elem, const uint16_t elem_size) {
    uint8_t ret = 0;

    if (elem_size > 0 && elem_size < SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE &&
        spiffs_circular_queue_available_space(cq) >= elem_size) 
    {
        if (_write_medium(cq, elem, elem_size)) {
            cq->back_idx = (cq->back_idx + sizeof(elem_size) + elem_size) % SPIFFS_CIRCULAR_QUEUE_MAX_DATA_SIZE;
            cq->count++;
            _spiffs_circular_queue_persist(cq);
            ret = 1;
        }
    }

    return ret;
}

uint8_t spiffs_circular_queue_dequeue(circular_queue_t *cq, uint8_t *elem, uint16_t *elem_size) {
    uint8_t ret = 0;

    if (!spiffs_circular_queue_is_empty(cq)) {
        // read last elem size
        uint16_t back_elem_size = 0;
        if (_read_medium(cq, elem, &back_elem_size)) {
            // advance front_idx index
            cq->front_idx = (cq->front_idx + sizeof(back_elem_size) + back_elem_size) % SPIFFS_CIRCULAR_QUEUE_MAX_DATA_SIZE;
            cq->count--;
            if (elem_size) *elem_size = back_elem_size;
            _spiffs_circular_queue_persist(cq);
            ret = 1;
        }
    }

    return ret;
}

uint8_t spiffs_circular_queue_is_empty(const circular_queue_t *cq) {
    return !cq->count;
}

uint32_t spiffs_circular_queue_size(const circular_queue_t *cq) {
    uint32_t qsize = 0;

    if (cq->back_idx > cq->front_idx) {
        qsize = cq->back_idx - cq->front_idx - cq->count*sizeof(uint16_t);
    } else if (cq->back_idx < cq->front_idx) {
        qsize = SPIFFS_CIRCULAR_QUEUE_MAX_DATA_SIZE - cq->front_idx + cq->back_idx - cq->count*sizeof(uint16_t);
    } else if (cq->count) { // && indices are equal
        qsize = SPIFFS_CIRCULAR_QUEUE_MAX_DATA_SIZE - cq->count*sizeof(uint16_t);
    }

    return qsize;
}

uint32_t spiffs_circular_queue_available_space(const circular_queue_t *cq) {
    // allows to return a real estimate for the next element 
    //   to be enqueued.
    // not upper limited by the SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE.
    //   Check this value to see max elem size you can enqueue.
    uint32_t gross_available_space = SPIFFS_CIRCULAR_QUEUE_MAX_DATA_SIZE - 
                                (spiffs_circular_queue_size(cq) + cq->count*sizeof(uint16_t));
    return gross_available_space <= sizeof(uint16_t) ? 0 : gross_available_space - sizeof(uint16_t);
}

uint32_t spiffs_circular_queue_get_front_idx(const circular_queue_t *cq) {
    return cq->front_idx;
}

uint32_t spiffs_circular_queue_get_back_idx(const circular_queue_t *cq) {
    return cq->back_idx;
}

uint16_t spiffs_circular_queue_get_count(const circular_queue_t *cq) {
    return cq->count;
}

uint32_t spiffs_circular_queue_get_file_size(const circular_queue_t *cq) {
    struct stat sb;

    return stat(cq->fn, &sb) < 0 ? 0 : sb.st_size;
}

uint8_t spiffs_circular_queue_free(circular_queue_t *cq, const uint8_t unmount_spiffs) {
    uint8_t ret = 0;

    if (!remove(cq->fn)) {
        ret = 1;
        if (unmount_spiffs) ret = _unmount_spiffs();
        memset(cq, 0x0, sizeof(circular_queue_t));
    }

    return ret;
}

static uint8_t _spiffs_circular_queue_persist(const circular_queue_t *cq) {
    FILE *fd = NULL;
    uint8_t nwritten = 0;

    if ((fd = fopen(cq->fn, "r+b"))) {
        // write front_idx and back indices to the file's head
        nwritten += fwrite(&(cq->front_idx), 1, sizeof(cq->front_idx), fd);
        nwritten += fwrite(&(cq->back_idx), 1, sizeof(cq->back_idx), fd);
        nwritten += fwrite(&(cq->count), 1, sizeof(cq->count), fd);
    
        fclose(fd);
    }
    
    return (nwritten == SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET);
}

static uint8_t _mount_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = SPIFFS_MAX_FILES_COUNT,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    return (ret == ESP_OK);
}

static uint8_t _unmount_spiffs(void) {
    return (esp_vfs_spiffs_unregister(NULL) == ESP_OK);
}

static uint8_t _write_medium(const circular_queue_t *cq, const uint8_t *data, const uint16_t data_size) {
    // spiffs medium
    FILE *fd = NULL;
    uint16_t nwritten = 0;

    if ((fd = fopen(cq->fn, "r+b"))) {
        uint32_t next_back_idx = SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back_idx;
        fseek(fd, next_back_idx, SEEK_SET);

        // case 1: split elem size
        if (next_back_idx + sizeof(data_size) > SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE) {
            uint8_t buf[sizeof(data_size)];
            // transform data_size into byte array
            memcpy(buf, &data_size, sizeof(data_size));
            // write first part of data_size
            nwritten = fwrite(buf, 1, SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - next_back_idx, fd);
            // set seek to the first usable byte
            fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET, SEEK_SET);
            // write the rest of data_size
            nwritten += fwrite(&buf[SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back_idx)], 1, 
                sizeof(data_size) - (SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back_idx)), fd);
            next_back_idx = SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + sizeof(data_size) - (SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->back_idx));
        } else { // normal write
            nwritten = fwrite(&data_size, 1, sizeof(data_size), fd);
            next_back_idx += sizeof(data_size);
        }
        // case 2: split elem data
        if (next_back_idx + data_size > SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE) {
            nwritten += fwrite(data, 1, SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - next_back_idx, fd);
            fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET, SEEK_SET);
            nwritten += fwrite(&data[SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - next_back_idx], 1, 
                data_size - (SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - next_back_idx), fd);
        } else { // normal write
            nwritten += fwrite(data, 1, data_size, fd);
        }
        
        fclose(fd);
    }

    return (nwritten == (sizeof(data_size) + data_size));
}

// read only non-null-pointer data and data_size
static uint8_t _read_medium(const circular_queue_t *cq, uint8_t *data, uint16_t *data_size) {
    // spiffs medium
    FILE *fd = NULL;
    uint16_t nread = 0;

    if ((fd = fopen(cq->fn, "r+b"))) {
        uint32_t next_front_idx = SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front_idx;
        fseek(fd, next_front_idx, SEEK_SET);

        // case 1: split elem size
        if (data_size) {
            if (next_front_idx + sizeof(*data_size) > SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE) {
                uint8_t buf[sizeof(*data_size)];
                // read first half
                nread = fread(buf, 1, SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front_idx), fd);
                // set seek to the first usable byte
                fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET, SEEK_SET);
                // read the rest of size
                nread += fread(&buf[SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front_idx)], 1, 
                    sizeof(*data_size) - (SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front_idx)), fd);
                // transform read by bytes data_size into uint16_t
                memcpy(data_size, buf, sizeof(*data_size));
                next_front_idx = SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + sizeof(*data_size) - (SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - (SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET + cq->front_idx));
            } else { // normal read
                nread = fread(data_size, 1, sizeof(*data_size), fd);
                next_front_idx += sizeof(*data_size);
            }
        }

        if (data) {
            // case 2: split elem data
            if (next_front_idx + *data_size > SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE) {
                nread += fread(data, 1, SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - next_front_idx, fd);
                // set seek to the first usable byte
                fseek(fd, SPIFFS_CIRCULAR_QUEUE_DATA_OFFSET, SEEK_SET);
                // read the rest of data
                nread += fread(&data[SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - next_front_idx], 1, 
                    *data_size - (SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE - next_front_idx), fd);
            } else { // normal read
                nread += fread(data, 1, *data_size, fd);
            }
        }

        fclose(fd);
    }

    return (nread == (data_size? sizeof(*data_size) : 0) + (data && data_size? *data_size : 0));
}