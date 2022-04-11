/**
* @file spiffs_circular_queue.h
* SPIFFS Circular Queue (aka FIFO) header file.
* @author rykovv
**/

#ifndef __SPIFFS_CIRCULAR_QUEUE__H__
#define __SPIFFS_CIRCULAR_QUEUE__H__

#define SPIFFS_MAX_FILES_COUNT                    (3)    ///< Maximum queue files that could open at the same time.
#define SPIFFS_CIRCULAR_QUEUE_FILE_MAX_SIZE       (4096) ///< Maximum file size for storing data, in bytes. Set your limit
#define SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE       (256)  ///< Queue elem size upper limit.
#define SPIFFS_FILE_NAME_MAX_SIZE                 (32)   ///< SPIFFS maximum allowable file name length

#include <Arduino.h>

typedef struct {
    char fn[SPIFFS_FILE_NAME_MAX_SIZE]; ///< Path to store the queue data in SPIFFS.
    uint32_t front = 0;                 ///< Queue front byte index
    uint32_t back = 0;                  ///< Queue back byte index
    uint16_t count = 0;                 ///< Queue nodes count
} circular_queue_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Initializes the library creating/reading a spiffs data file. Sets current front, back, and count queue indices.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_init(circular_queue_t *cq);

/**
 *	Copies front queue element of elem_size size to the elem.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *	@param[out] elem 		Pointer to a queue element buffer
 *  @param[out] elem_size   Pointer to a queue element size
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_front(const circular_queue_t *cq, uint8_t *elem, uint16_t *elem_size);

/**
 *	Enqueues elem of elem_size size to the front of the queue.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *	@param[out] elem 		Pointer to a queue element buffer
 *  @param[out] elem_size   A queue element size
 *
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_enqueue(circular_queue_t *cq, const uint8_t *elem, const uint16_t elem_size);

/**
 *	Pops out the first element of the queue. When elem and elem_size are valid pointers, front element is placed in them and then it pops out.
 *
 *  @param[in] cq 			Pointer to the circular_queue_t struct
 *  @param[out] elem        Pointer to a queue element buffer
 *  @param[out] elem_size   A queue element size
 * 
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_dequeue(circular_queue_t *cq, uint8_t *elem = NULL, uint16_t *elem_size = NULL);

/**
 *	Checks whether the queue is empty or not.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					1 when empty and 0 if not
 */
uint8_t spiffs_circular_queue_is_empty(const circular_queue_t *cq);

/**
 *	Returns the current queue pure data size.
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
 *	@return					queue nodes count
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
