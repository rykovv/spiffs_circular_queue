/**
* @file spiffs_circular_queue.h
* SPIFFS Circular Queue (aka FIFO) header file.
* @author rykovv
**/

#ifndef __SPIFFS_CIRCULAR_QUEUE_MANAGER__H__
#define __SPIFFS_CIRCULAR_QUEUE_MANAGER__H__

#define SPIFFS_CIRCULAR_QUEUE_MAX_SIZE            (4096) ///< Maximum file size for storing data, in bytes. Set your limit
#define SPIFFS_CIRCULAR_QUEUE_ITEM_SIZE           (256)
#define SPIFFS_FILE_NAME_MAX_SIZE                 (32)   ///< SPIFFS maximum allowable file name length

#include <Arduino.h>

typedef struct {
    char fn[SPIFFS_FILE_NAME_MAX_SIZE] = 'spiffs/data'; ///< Path to store the queue data in SPIFFS
    uint32_t front = 0;                                 ///< Queue front index
    uint32_t back = 0;                                  ///< Queue back index
} circular_queue_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Initializes the library and sets current front and back queue indices (useful for ESP32 MCU)
 *
 *	@param[in] front 			Index to current front element
 *	@param[in] back 			Index to current back element
 *	@return					1 on success and 0 on fail
 */
uint8_t 	spiffs_circular_queue_init(uint16_t front = 0, uint16_t back = 0);

/**
 *	Copies front element of the queue into elem pointer.
 *
 *	@param[out] front 		Pointer to buffer
 *
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_front(uint8_t *elem);

/**
 *	Enqueues elem to the front of the queue.
 *
 *	@param[in] front 		Pointer to buffer
 *
 *	@return					1 on success and 0 on fail
 */
uint8_t spiffs_circular_queue_enqueue(uint8_t *elem);

/**
 *	Pops out the first element of the queue.
 */
void 	spiffs_circular_queue_dequeue(void);

/**
 *	Checks whether the queue is empty or not.
 *
 *	@return					1 when empty and 0 if not
 */
uint8_t spiffs_circular_queue_is_empty(void);

/**
 *	Returns the current queue size according to the current indices.
 *
 *	@return					queue size
 */
uint16_t spiffs_circular_queue_size(void);

/**
 *	Gets the front index of the queue
 *
 *	@return					front index
 */
uint16_t spiffs_circular_queue_get_front_indx(void);

/**
 *	Gets the back index of the queue
 *
 *	@return					back index
 */
uint16_t spiffs_circular_queue_get_back_indx(void);

/**
 *	Frees resourses allocated for the queue and closes the SPIFFS.
 *
 *	@return					1 when empty and 0 if not
 */
uint8_t	spiffs_circular_queue_free(void);

#ifdef __cplusplus
}
#endif

#endif // __SPIFFS_CIRCULAR_QUEUE_MANAGER__H__
