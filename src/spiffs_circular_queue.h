/**
* @file spiffs_circular_queue.h
* SPIFFS Circular Queue (aka FIFO) header file.
* @author rykovv
**/

#ifndef __SPIFFS_CIRCULAR_QUEUE__H__
#define __SPIFFS_CIRCULAR_QUEUE__H__

#define SPIFFS_CIRCULAR_QUEUE_MAX_SIZE            (4096) ///< Maximum file size for storing data, in bytes. Set your limit
#define SPIFFS_CIRCULAR_QUEUE_MAX_ELEM_SIZE       (256)  ///< Queue elem size upper limit.
#define SPIFFS_FILE_NAME_MAX_SIZE                 (32)   ///< SPIFFS maximum allowable file name length

#include <Arduino.h>

typedef struct {
    char fn[SPIFFS_FILE_NAME_MAX_SIZE]; ///< Path to store the queue data in SPIFFS
    uint32_t front = 0;                 ///< Queue front byte index
    uint32_t back = 0;                  ///< Queue back byte index
} circular_queue_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Initializes the library creating/reading a spiffs data file. Sets current front and back queue indices.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
  *	@param[in] mount_spiffs Mount SPIFFS on init flag
 *
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_init(circular_queue_t *cq, const uint8_t mount_spiffs = 1);

/**
 *	Copies front queue element of elem_size size to the elem.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *	@param[out] elem 		Pointer to a queue element buffer
 *  @param[out] elem_size   Pointer to a queue element size
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_front(const circular_queue_t *cq, uint8_t *elem, uint32_t *elem_size);

/**
 *	Enqueues elem of elem_size size to the front of the queue.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *	@param[out] elem 		Pointer to a queue element buffer
 *  @param[out] elem_size   A queue element size
 *
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_enqueue(circular_queue_t *cq, const uint8_t *elem, const uint32_t elem_size);

/**
 *	Pops out the first element of the queue.
 *
 *  @param[in] cq 			Pointer to the circular_queue_t struct
 */
void spiffs_circular_queue_dequeue(circular_queue_t *cq);

/**
 *	Checks whether the queue is empty or not.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					1 when empty and 0 if not
 */
uint8_t spiffs_circular_queue_is_empty(const circular_queue_t *cq);

/**
 *	Returns the current queue size according to the current indices.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					queue size
 */
uint16_t spiffs_circular_queue_size(const circular_queue_t *cq);

/**
 *	Gets the front index of the queue
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					front index
 */
uint16_t spiffs_circular_queue_get_front_indx(const circular_queue_t *cq);

/**
 *	Gets the back index of the queue
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					back index
 */
uint16_t spiffs_circular_queue_get_back_indx(const circular_queue_t *cq);

/**
 *	Frees resourses allocated for the queue and closes the SPIFFS.
 *
 *	@param[in] cq 			Pointer to the circular_queue_t struct
 *
 *	@return					1 when empty and 0 if not
 */
uint8_t	spiffs_circular_queue_free(const circular_queue_t *cq);

#ifdef __cplusplus
}
#endif

#endif // __SPIFFS_CIRCULAR_QUEUE__H__
