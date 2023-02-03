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
#error Library designed to work with ESP32 arch and x-tensa toolchain 
#endif

#define CIRCULAR_QUEUE_DATA_OFFSET_FIXED    (sizeof(uint32_t)*3 + \
                                            sizeof(uint16_t)    + \
                                            sizeof(uint8_t))    ///< Data location file offset (fixed part)


/// private function to mount SPIFFS during initialization
static uint8_t _mount_spiffs(void);
/// private function to unmount SPIFFS when you don't need it, i.e. before going in a sleep mode
static uint8_t _unmount_spiffs(void);
/// private function that adds write medium-independent abstraction
static uint8_t _write_medium(const circular_queue_t *cq, const void *data, const uint16_t data_size);
/// private function that adds read medium-independent abstraction. data = NULL to read only the size of last elem
static uint8_t _read_medium(const circular_queue_t *cq, void *data, uint16_t *data_size);
/// private function that saves current pointers to the queue file
static uint8_t _spiffs_circular_queue_persist(const circular_queue_t *cq);

static inline uint32_t _spiffs_circular_queue_full_size(const circular_queue_t *cq);
static inline uint8_t _circular_queue_get_data_offset(const circular_queue_t *cq);

uint8_t spiffs_circular_queue_init(circular_queue_t *cq) {
    uint8_t ret = 1;

    if (ret && !esp_spiffs_mounted(NULL)) {
        ret = _mount_spiffs();
    }

    if (ret) {
        struct stat sb;
        FILE *fd = NULL;

        // stat returns 0 upon succes (file exists) and -1 on failure (does not)
        if (stat(cq->fn, &sb) < 0) {
            if ((fd = fopen(cq->fn, "w"))) {

                cq->front_idx = cq->back_idx = 0;
                cq->count = 0;

                // set fixed elem size flags bit
                cq->flags.fields.fixed_elem_size = cq->elem_size > 0;
                
                // set default max size, if not specified
                if (!cq->max_size) cq->max_size = CIRCULAR_QUEUE_DEFAULT_MAX_SIZE;

                // set front and back indices in the file's head
                uint8_t nwritten = fwrite(&(cq->front_idx), 1, sizeof(cq->front_idx), fd);
                nwritten += fwrite(&(cq->back_idx), 1, sizeof(cq->back_idx), fd);
                nwritten += fwrite(&(cq->count), 1, sizeof(cq->count), fd);
                nwritten += fwrite(&(cq->max_size), 1, sizeof(cq->max_size), fd);
                nwritten += fwrite(&(cq->flags.value), 1, sizeof(cq->flags.value), fd);
                if (cq->elem_size) { // if fixed elem size
                    nwritten += fwrite(&(cq->elem_size), 1, sizeof(cq->elem_size), fd);
                }
                
                ret = nwritten == _circular_queue_get_data_offset(cq);
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
                nread += fread(&(cq->max_size), 1, sizeof(cq->max_size), fd);
                nread += fread(&(cq->flags.value), 1, sizeof(cq->flags.value), fd);
                if (cq->flags.fields.fixed_elem_size) {
                    nread += fread(&(cq->elem_size), 1, sizeof(cq->elem_size), fd);
                }

                ret = nread == _circular_queue_get_data_offset(cq);
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

uint8_t spiffs_circular_queue_front(const circular_queue_t *cq, void *elem, uint16_t *elem_size) {
    uint8_t ret = 0;

    if (!spiffs_circular_queue_is_empty(cq)) {
        ret = _read_medium(cq, elem, elem_size);
    }

    return ret;
}

uint8_t spiffs_circular_queue_enqueue(circular_queue_t *cq, const void *elem, const uint16_t elem_size) {
    uint8_t ret = 0;
    uint32_t enqueue_size = cq->elem_size? cq->elem_size : elem_size;

    if (enqueue_size && spiffs_circular_queue_available_space(cq) >= enqueue_size &&
        (!SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE ||
        (SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE && enqueue_size < SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE))
    ) {
        if (_write_medium(cq, elem, elem_size)) {
            enqueue_size += cq->elem_size? 0 : sizeof(elem_size);

            cq->back_idx = (cq->back_idx + enqueue_size) % cq->max_size;
            cq->count++;
            ret = _spiffs_circular_queue_persist(cq);
        }
    }

    return ret;
}

uint8_t spiffs_circular_queue_dequeue(circular_queue_t *cq, void *elem, uint16_t *elem_size) {
    uint8_t ret = 0;

    if (!spiffs_circular_queue_is_empty(cq)) {
        if (_read_medium(cq, elem, elem_size)) {
            uint16_t dequeued_size = cq->elem_size? cq->elem_size : (sizeof(*elem_size) + *elem_size);

            cq->front_idx = (cq->front_idx + dequeued_size) % cq->max_size;
            cq->count--;
            ret = _spiffs_circular_queue_persist(cq);
        }
    }

    return ret;
}

uint8_t spiffs_circular_queue_is_empty(const circular_queue_t *cq) {
    return !cq->count;
}

uint32_t spiffs_circular_queue_size(const circular_queue_t *cq) {
    uint32_t qsize = 0;

    uint32_t elem_size_total = cq->elem_size? 0 : cq->count*sizeof(uint16_t);

    if (cq->back_idx > cq->front_idx) {
        qsize = cq->back_idx - cq->front_idx - elem_size_total;
    } else if (cq->back_idx < cq->front_idx) {
        qsize = cq->max_size - cq->front_idx + cq->back_idx - elem_size_total;
    } else if (cq->count) { // && indices are equal
        qsize = cq->max_size - elem_size_total;
    }

    return qsize;
}

uint32_t spiffs_circular_queue_available_space(const circular_queue_t *cq) {
    uint32_t elem_size_total = 0;
    uint16_t next_elem_size = 0;

    if (!cq->elem_size) { // if variable elem size
        elem_size_total = cq->count*sizeof(uint16_t);
        next_elem_size = sizeof(uint16_t);
    }

    uint32_t gross_available_space = cq->max_size - 
                                (spiffs_circular_queue_size(cq) + elem_size_total);

    return gross_available_space <= next_elem_size ? 0 : gross_available_space - next_elem_size;
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

#define SPIFFS_CIRCULAR_QUEUE_PERSIST_SIZE  (sizeof(uint32_t)*2 + sizeof(uint16_t)) ///< Persist bytes
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
    
    return (nwritten == SPIFFS_CIRCULAR_QUEUE_PERSIST_SIZE);
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

// not null-pointer safe
static uint8_t _write_medium(const circular_queue_t *cq, const void *data, const uint16_t data_size) {
    // spiffs medium
    FILE *fd = NULL;
    uint16_t nwritten = 0;

    if ((fd = fopen(cq->fn, "r+b"))) {
        uint32_t next_back_idx = _circular_queue_get_data_offset(cq) + cq->back_idx;
        fseek(fd, next_back_idx, SEEK_SET);

        if (!cq->elem_size) { // if fixed elem size
            // case 1: split elem size
            if (next_back_idx + sizeof(data_size) > _spiffs_circular_queue_full_size(cq)) {
                uint8_t buf[sizeof(data_size)];
                // transform data_size into byte array
                memcpy(buf, &data_size, sizeof(data_size));
                // write first part of data_size
                nwritten = fwrite(buf, 1, _spiffs_circular_queue_full_size(cq) - next_back_idx, fd);
                // set seek to the first usable byte
                fseek(fd, _circular_queue_get_data_offset(cq), SEEK_SET);
                // write the rest of data_size
                nwritten += fwrite(&buf[cq->max_size - cq->back_idx], 1, 
                    sizeof(data_size) - (cq->max_size - cq->back_idx), fd);
                next_back_idx = _circular_queue_get_data_offset(cq) + sizeof(data_size) - (cq->max_size - cq->back_idx);
            } else { // normal write
                nwritten = fwrite(&data_size, 1, sizeof(data_size), fd);
                next_back_idx += sizeof(data_size);
            }
        }
        // case 2: split elem data
        if (data) {
            uint16_t write_size = cq->elem_size? cq->elem_size : data_size;

            if (next_back_idx + write_size > _spiffs_circular_queue_full_size(cq)) {
                uint8_t *data_bp = (uint8_t *)data; // byte-pointer

                nwritten += fwrite(data_bp, 1, _spiffs_circular_queue_full_size(cq) - next_back_idx, fd);
                fseek(fd, _circular_queue_get_data_offset(cq), SEEK_SET);
                nwritten += fwrite(&data_bp[_spiffs_circular_queue_full_size(cq) - next_back_idx], 1, 
                    write_size - (_spiffs_circular_queue_full_size(cq) - next_back_idx), fd);
            } else { // normal write
                nwritten += fwrite(data, 1, write_size, fd);
            }
        }

        fclose(fd);
    }

    return (nwritten == (cq->elem_size? cq->elem_size : (sizeof(data_size) + data_size)));
}

// read only non-null-pointer data and data_size. null-poiner safe
static uint8_t _read_medium(const circular_queue_t *cq, void *data, uint16_t *data_size) {
    // spiffs medium
    uint8_t ret = 1;

    FILE *fd = NULL;
    uint16_t nread = 0;

    if ((fd = fopen(cq->fn, "r+b"))) {
        uint32_t next_front_idx = _circular_queue_get_data_offset(cq) + cq->front_idx;
        fseek(fd, next_front_idx, SEEK_SET);

        if (!cq->elem_size) { // if fixed elem size
            if (data_size) { // case 1: split elem size
                if (next_front_idx + sizeof(*data_size) > _spiffs_circular_queue_full_size(cq)) {
                    uint8_t buf[sizeof(*data_size)];
                    // read first half
                    nread = fread(buf, 1, cq->max_size - cq->front_idx, fd);
                    // set seek to the first usable byte
                    fseek(fd, _circular_queue_get_data_offset(cq), SEEK_SET);
                    // read the rest of size
                    nread += fread(&buf[cq->max_size - cq->front_idx], 1, sizeof(*data_size) - (cq->max_size - cq->front_idx), fd);
                    // transform read by bytes data_size into uint16_t
                    memcpy(data_size, buf, sizeof(*data_size));
                    next_front_idx = _circular_queue_get_data_offset(cq) + sizeof(*data_size) - (cq->max_size - cq->front_idx);
                } else { // normal read
                    nread = fread(data_size, 1, sizeof(*data_size), fd);
                    next_front_idx += sizeof(*data_size);
                }
            } else {
                ret = 0;
            }
        }

        if (data) {
            uint16_t read_size = cq->elem_size? cq->elem_size : *data_size;

            // case 2: split elem data
            if (next_front_idx + read_size > _spiffs_circular_queue_full_size(cq)) {
                uint8_t *data_bp = (uint8_t *)data; // byte-pointer

                nread += fread(data_bp, 1, _spiffs_circular_queue_full_size(cq) - next_front_idx, fd);
                // set seek to the first usable byte
                fseek(fd, _circular_queue_get_data_offset(cq), SEEK_SET);
                // read the rest of data
                nread += fread(&data_bp[_spiffs_circular_queue_full_size(cq) - next_front_idx], 1, 
                    read_size - (_spiffs_circular_queue_full_size(cq) - next_front_idx), fd);
            } else { // normal read
                nread += fread(data, 1, read_size, fd);
            }
        } else {
            ret = 0;
        }

        fclose(fd);
    }

    if (ret) { // pointer validity checked
        // check read bytes size
        uint16_t exp_read_size = cq->elem_size? cq->elem_size : (sizeof(*data_size) + *data_size);
        ret = nread == exp_read_size;
    }

    return ret;
}

static inline uint32_t _spiffs_circular_queue_full_size(const circular_queue_t *cq) {
    return (cq->max_size + _circular_queue_get_data_offset(cq));
}

static inline uint8_t _circular_queue_get_data_offset(const circular_queue_t *cq) {
    uint8_t ret = CIRCULAR_QUEUE_DATA_OFFSET_FIXED;

    if (cq->flags.fields.fixed_elem_size) {
        ret += sizeof(uint16_t);
    }

    return ret;
}