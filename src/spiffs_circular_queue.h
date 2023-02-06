/**
* @file spiffs_circular_queue.h
* SPIFFS Circular Queue (aka FIFO) header file.
* @author rykovv
**/

#ifndef __SPIFFS_CIRCULAR_QUEUE__H__
#define __SPIFFS_CIRCULAR_QUEUE__H__

#define SPIFFS_MAX_FILES_COUNT                    (3u)    ///< Maximum queue files that could open at the same time.
#define SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE       (0u)    ///< Queue elem size upper limit. 0 if disabled
#define SPIFFS_FILE_NAME_MAX_SIZE                 (32u)   ///< SPIFFS maximum allowable file name length
#define CIRCULAR_QUEUE_DEFAULT_MAX_SIZE           (2048u) ///< Default queue max size in bytes

#include <Arduino.h>

typedef struct _circular_queue_t circular_queue_t;

/// Queue types enum, it will go populating with the development of the project
typedef enum {
    CIRCULAR_QUEUE_TYPE_SPIFFS = 0,
} circular_queue_type_t;

/// Union with a bitfield for easy access to queue flags
typedef union {
    struct {
        unsigned char queue_type        : 4;
        unsigned char reserved          : 3;
        unsigned char fixed_elem_size   : 1;
    } fields;
    unsigned char value;
} circular_queue_flags_t;

/// Main queue struct
typedef struct _circular_queue_t {
    char fn[SPIFFS_FILE_NAME_MAX_SIZE]; ///< Path to store the queue data in SPIFFS. Mandatory prefix "/spiffs/"
    uint32_t front_idx;             ///< Queue front byte index
    uint32_t back_idx;              ///< Queue back byte index
    uint16_t count;                 ///< Queue nodes count

    uint32_t max_size;              ///< Queue max data size in bytes
    uint16_t elem_size;             ///< Queue fixed elem size in bytes

    circular_queue_flags_t flags;   ///< Flags for queue type, fixed elem size, etc

    // Function pointers to get oo flavour 
    uint8_t (*front)(const circular_queue_t*, void*, uint16_t*);
    uint8_t (*enqueue)(circular_queue_t*, const void*, const uint16_t);
    uint8_t (*dequeue)(circular_queue_t*, void*, uint16_t*);
    uint8_t (*is_empty)(const circular_queue_t*);
    uint32_t (*size)(const circular_queue_t*);
    uint32_t (*available_space)(const circular_queue_t*);
    uint32_t (*get_front_idx)(const circular_queue_t*);
    uint32_t (*get_back_idx)(const circular_queue_t*);
    uint16_t (*get_count)(const circular_queue_t*);
    uint32_t (*get_file_size)(const circular_queue_t*);
    uint8_t (*free)(circular_queue_t*, uint8_t);
} _circular_queue_t;

/**
 *	Macro that resembles foreach loop behaviour. Pops out the last queue elem
 *  each loop cycle until the queue is empty.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *	@param[out] elem 		Pointer to a queue elem buffer
 *  @param[out] elem_size   Pointer to a queue elem size
 * 
 */ 
#define spiffs_circular_queue_foreach_dequeue(cq, elem, elem_size)      \
    while (spiffs_circular_queue_dequeue(cq, elem, elem_size))         \


#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Initializes the library creating/reading a spiffs data file. Sets current front, back, and count queue indices.
 *
 *  For non-volatile storages the initialization and re-initialization depend on the written queue data.
 *  The library will set its indices and count variables to what is encounted on the medium or set to zeros
 *  if found none. If you modify cq struct variables outside of the library, that will be lost if _persist 
 *  function will not be called.
 *  Initialization will result in failure only on null cq struct pointer, failure to mount SPIFFS, or failure
 *  to write queue data file on SPIFFS.
 *
 *	@param[in] cq 	        Pointer to the circular_queue_t struct
 *
 *	@return			        1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_init(circular_queue_t *cq);

/**
 *	Places front queue elem of elem_size size to the elem.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *	@param[out] elem 		Pointer to a queue elem buffer
 *  @param[out] elem_size   Pointer to a queue elem size.
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_front(const circular_queue_t *cq, void *elem = NULL, uint16_t *elem_size = NULL);

/**
 *	Enqueues elem of elem_size size to the front of the queue if there is enough room in the queue.
 *
 *  Be responsible for passing elem buffer of SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE size or less 
 *  if SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE is enabled
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *	@param[out] elem 		Pointer to a queue elem buffer
 *  @param[out] elem_size   A queue elem size
 *
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_enqueue(circular_queue_t *cq, const void *elem = NULL, const uint16_t elem_size = 0);

/**
 *	Pops out the first elem of the queue. When elem and elem_size are valid pointers, front elem is placed in them and then it pops out.
 *
 *  @param[in] cq 			Pointer to the circular_queue_t struct
 *  @param[out] elem        Pointer to a queue elem buffer
 *  @param[out] elem_size   A queue elem size
 * 
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_dequeue(circular_queue_t *cq, void *elem = NULL, uint16_t *elem_size = NULL);

/**
 *	Checks whether the queue is empty or not.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					1 when empty and 0 if not
 */
uint8_t spiffs_circular_queue_is_empty(const circular_queue_t *cq);

/**
 *	Returns the current queue net data size.
 *
 *  This function is useful when you need to know how much pure data
 *  you still need to process/send. It does not account nodes sizes
 *  stored at head of each node.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					queue size in bytes
 */
uint32_t spiffs_circular_queue_size(const circular_queue_t *cq);

/**
 *	Returns queue available space in bytes.
 *
 *  Allows to return a real estimate for the next elem to be enqueued.
 *  Caution: not upper limited by the SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE.
 *  Check SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE value to see max elem size 
 *  you can enqueue if enabled.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					queue available space in bytes
 */
uint32_t spiffs_circular_queue_available_space(const circular_queue_t *cq);

/**
 *	Gets the front index of the queue
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					front index
 */
uint32_t spiffs_circular_queue_get_front_idx(const circular_queue_t *cq);

/**
 *	Gets the back index of the queue
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					back index
 */
uint32_t spiffs_circular_queue_get_back_idx(const circular_queue_t *cq);

/**
 *	Gets the queue nodes count
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					queue nodes count
 */
uint16_t spiffs_circular_queue_get_count(const circular_queue_t *cq);

/**
 *	Gets the queue SPIFFS file footprint in bytes
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					SPIFFS queue file size in bytes
 */
uint32_t spiffs_circular_queue_get_file_size(const circular_queue_t *cq);

/**
 *	Frees resourses allocated for the queue and closes the SPIFFS.
 *
 *	@param[in] cq 			    Pointer to the circular_queue_t struct
 *	@param[in] unmount_spiffs   Unmount SPIFFS on free flag
 *
 *	@return					    1 on success and 0 on fail
 */
uint8_t	spiffs_circular_queue_free(circular_queue_t *cq, const uint8_t unmount_spiffs = 1);

#ifdef __cplusplus
}
#endif

#endif // __SPIFFS_CIRCULAR_QUEUE__H__
