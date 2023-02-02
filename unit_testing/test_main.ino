/**
* @file test_main.cpp
* SPIFFS Circular Queue (aka FIFO) unit tests file.
* This file contains thourough unit testing of the circular queue library.
* The unit testing methodology was approproately adopted.
* @author rykovv
**/

#include "spiffs_circular_queue/src/spiffs_circular_queue.h"

/*
 *  Test cases:
 *      I) Variable size queue
 *          1) [done] Empty queue
 *              (a) [done] init
 *              (b) [done] double init failure
 *          2) [done] Non-empty queue 
 *          3) [done] Enqueuing element larger than available space
 *          4) [done] Full queue
 *          5) [done] Wrap around behavior
 *          6) [done] Dequeue empty queue
 *          7) Dequeue non-empty queue
 *              (a) [done] normal
 *              (c) foreach_dequeue (next release)
 *          8) Traverse. foreach (not implemented. value vs complexity)
 *          9) [done] Make two queues 
 *          10) [done] size function
 *          11) [done] available_space function
 *          12) [done] get_count function
 *          13) [done] front function
 *          14) [done] is_empty function
 *          15) 
 *          ...
 *          n-4) dequeue to empty implicitly done many times in present test cases
 *          n-3) enqueue and dequeue functions are implicitly tested
 *          n-2) get front and back indices functions are implicitely tested
 *          n-1) get_file_size is not relevant to functionality. Not necessary to test
 *          n) free is tested every time in tear_down
 *      I) Fixed size queue
 *          1) Empty queue
 *              (a) init
 *              (b) double init failure
 *          2) Non-empty queue 
 *          3) Enqueuing element larger than available space
 *          4) Full queue
 *          5) Wrap around behavior
 *          6) Dequeue empty queue
 *          7) Dequeue non-empty queue
 *              (a) normal
 *              (c) foreach_dequeue (next release)
 *          8) Traverse. foreach (next release)
 *          9) Make two queues 
 *          10) size function
 *          11) available_space function
 *          12) get_count function
 *          13) front function
 *          14) is_empty function
 * 
 *  Each test case must be tested on every medium (SPIFFS, EEPROM, RAM)
*/

/*
 * To reduce possibility of a mistake each element in the queue will be a series of natural numbers.
 * This way the tests can be parameterized and scaled while proving correct functionality.
 * Knowing size of an elem sum of its bytes can be calculated and checked against known formula result.
 * The unique concern is that the size of each elem should be less than 256.
*/

#define CIRCULAR_QUEUE_NAME             "/spiffs/test"
#define CIRCULAR_QUEUE_MAX_ELEM_SIZE    80 // will fill ~3500 bytes


circular_queue_t cq;

// Unit test functions declarations
void set_up(void);
void tear_down(void);
void assert_equal(unsigned expected, unsigned actual, const char *message);
void run_test(void (* test)(void));

// data generation function declaration
void _makeseq(uint16_t n, uint8_t *arr, uint16_t arr_size);


// Unit test functions definitions
void set_up(void) {
    snprintf(cq.fn, SPIFFS_FILE_NAME_MAX_SIZE, CIRCULAR_QUEUE_NAME);

    if (spiffs_circular_queue_init(&cq)) {
        printf("--------------- Setup properly done.\n");
    } else {
        printf("--------------- Setup didn't work.\n");
        spiffs_circular_queue_free(&cq, 0);
    }
}

void tear_down(void) {
    if (cq.free(&cq, 1)) {
        printf("--------------- Tear down properly done.\n");
    } else {
        printf("--------------- Tear down didn't work.\n");
    }
}

void assert_equal(unsigned expected, unsigned actual, const char *message) {
    if (expected == actual) {
        printf("[PASS]: %s\n", message);
    } else {
        printf("[FAIL]: %s\n", message);
    }
}

void run_test(void (* test_func)(void)) {
    set_up();
    (*test_func)();
    tear_down();
}

// Data generation function definition
void _makeseq(uint16_t n, uint8_t *arr, uint16_t arr_size) {
    for (uint16_t i = 1; i <= n && i < arr_size; i++) {
        arr[i] = i;
    }
}

// Test Cases

// SPIFFS medium test cases

void spiffs_empty_queue_init_variable(void) {
    assert_equal(cq.front_idx | cq.back_idx | cq.count | cq.size(&cq), 0, "SPIFFS Empty Queue Init. struct variables checked.");
}

void spiffs_empty_queue_double_init_variable(void) {
    assert_equal(spiffs_circular_queue_init(&cq), 1, "SPIFFS Empty Queue Double Init. Reinitialization is fine and must be controlled by the user.");
}

void spiffs_non_empty_queue_variable(void) {
    uint8_t buf[CIRCULAR_QUEUE_MAX_ELEM_SIZE +1];
    uint16_t buf_size = 0;
    uint32_t expc_sum = 0;
    uint32_t real_sum = 0;

    for (uint16_t n = 1; n <= CIRCULAR_QUEUE_MAX_ELEM_SIZE; n++) {
        _makeseq(n, buf, CIRCULAR_QUEUE_MAX_ELEM_SIZE+1);
        if (cq.enqueue(&cq, buf, n)) {
            for (uint16_t i = 0; i < n; i++) {
                expc_sum += buf[i];
            }
        } else {
            printf("Failed on enqueuing %dth element. Queue size is %d\n", n, cq.size(&cq));
        }
    }

    while (!cq.is_empty(&cq)) {
        cq.dequeue(&cq, buf, &buf_size);
        for (uint16_t i = 0; i < buf_size; i++) {
            real_sum += buf[i];
        }
    }

    assert_equal(expc_sum, real_sum, "SPIFFS Non-Empty Queue. Enqueued elements and checksum checked");
    printf("        Expected/Real (%d/%d)\n", expc_sum, real_sum);
}

#define SPIFFS_FULL_QUEUE_ELEM_SIZE 255
void spiffs_enqueueing_elem_larger_than_available_space_variable(void) {
    uint8_t buf[SPIFFS_FULL_QUEUE_ELEM_SIZE+1];

    _makeseq(SPIFFS_FULL_QUEUE_ELEM_SIZE, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE+1);
    
    while (cq.available_space(&cq) >= SPIFFS_FULL_QUEUE_ELEM_SIZE) {
        cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);
    }

    assert_equal(0, !cq.is_empty(&cq) && cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE), "SPIFFS Enqueue Elem Larger Than Aval Space. Enqueueing on a full queue.");
}

// takes a while
void spiffs_wrap_around_variable(void) {
    uint8_t buf[CIRCULAR_QUEUE_MAX_ELEM_SIZE +1];
    uint16_t buf_size = 0;
    uint32_t expc_sum = 0;
    uint32_t real_sum = 0;
    uint16_t n = 1;

    while(cq.available_space(&cq) >= n) {
        _makeseq(n, buf, CIRCULAR_QUEUE_MAX_ELEM_SIZE+1);
        if (cq.enqueue(&cq, buf, n)) {
            for (uint16_t i = 0; i < n; i++) {
                expc_sum += buf[i];
            }
        } else {
            printf("1 Failed on enqueuing %dth element. Queue size is %d\n", n, cq.size(&cq));
        }

        if (n == CIRCULAR_QUEUE_MAX_ELEM_SIZE) {
            n = 1;
        } else {
            n++;
        }
    }
    // here the queue is full, thus the file size is or close to max. 
    //   need to dequeue a couple of elems and then enqueue again until full
    
    uint16_t halfcount = cq.get_count(&cq)/2;
    uint32_t tmp_sum = 0;
    for (uint16_t i = 0; i < halfcount; i++) {
        cq.dequeue(&cq, buf, &buf_size);
        for (uint16_t j = 0; j < buf_size; j++) {
            tmp_sum += buf[j];
        }
    }
    expc_sum -= tmp_sum;

    n = 1;
    while(cq.available_space(&cq) >= n) {
        _makeseq(n, buf, CIRCULAR_QUEUE_MAX_ELEM_SIZE+1);
        if (cq.enqueue(&cq, buf, n)) {
            for (uint16_t i = 0; i < n; i++) {
                expc_sum += buf[i];
            }
        } else {
            printf("2 Failed on enqueuing %dth element. Queue size is %d\n", n, cq.size(&cq));
        }

        if (n == CIRCULAR_QUEUE_MAX_ELEM_SIZE) {
            n = 1;
        } else {
            n++;
        }
    }

    while (!cq.is_empty(&cq)) {
        cq.dequeue(&cq, buf, &buf_size);
        for (uint16_t i = 0; i < buf_size; i++) {
            real_sum += buf[i];
        }
    }

    assert_equal(expc_sum, real_sum, "SPIFFS Wrap Around. Enqueued elements until full, dequeue half queue, enqueue again until full, and check checksum.");
    printf("        Expected/Real (%d/%d)\n", expc_sum, real_sum);
}

void spiffs_dequeue_empty_variable(void) {
    uint8_t buf[CIRCULAR_QUEUE_MAX_ELEM_SIZE +1];
    uint16_t buf_size = 0;

    assert_equal(0, cq.dequeue(&cq, buf, &buf_size), "SPIFFS Dequeue Empty. Failure Expected.");
}

void spiffs_dequeue_nonempty_variable(void) {
    uint8_t buf[CIRCULAR_QUEUE_MAX_ELEM_SIZE +1];
    uint16_t buf_size = 0;
    uint32_t expc_sum = 0;
    uint32_t real_sum = 0;
    uint16_t n = 1;

    while(cq.available_space(&cq) >= n) {
        _makeseq(n, buf, CIRCULAR_QUEUE_MAX_ELEM_SIZE+1);
        if (cq.enqueue(&cq, buf, n)) {
            for (uint16_t i = 0; i < n; i++) {
                expc_sum += buf[i];
            }
        } else {
            printf("Failed on enqueuing1q; %dth element. Queue size is %d\n", n, cq.size(&cq));
        }

        if (n == CIRCULAR_QUEUE_MAX_ELEM_SIZE) {
            n = 1;
        } else {
            n++;
        }
    }
    // here the queue is full, thus the file size is or close to max. 
    //   need to dequeue a couple of elems
    
    uint16_t halfcount = cq.get_count(&cq)/2;
    uint32_t tmp_sum = 0;
    for (uint16_t i = 0; i < halfcount; i++) {
        cq.dequeue(&cq, buf, &buf_size);
        for (uint16_t j = 0; j < buf_size; j++) {
            tmp_sum += buf[j];
        }
    }
    expc_sum -= tmp_sum;

    while (!cq.is_empty(&cq)) {
        cq.dequeue(&cq, buf, &buf_size);
        for (uint16_t i = 0; i < buf_size; i++) {
            real_sum += buf[i];
        }
    }

    assert_equal(expc_sum, real_sum, "SPIFFS Dequeue Non-Empty. Enqueued elements until full, dequeue half queue, enqueue again until full, and check checksum.");
    printf("        Expected/Real (%d/%d)\n", expc_sum, real_sum);
}

void spiffs_dequeue_nonempty_foreach_variable(void) {
    uint8_t buf[CIRCULAR_QUEUE_MAX_ELEM_SIZE +1];
    uint16_t buf_size = 0;
    uint32_t expc_sum = 0;
    uint32_t real_sum = 0;
    uint16_t n = 1;

    while(cq.available_space(&cq) >= n) {
        _makeseq(n, buf, CIRCULAR_QUEUE_MAX_ELEM_SIZE+1);
        if (cq.enqueue(&cq, buf, n)) {
            for (uint16_t i = 0; i < n; i++) {
                expc_sum += buf[i];
            }
        } else {
            printf("Failed on enqueuing1q; %dth element. Queue size is %d\n", n, cq.size(&cq));
        }

        if (n == CIRCULAR_QUEUE_MAX_ELEM_SIZE) {
            n = 1;
        } else {
            n++;
        }
    }
    // here the queue is full, thus the file size is or close to max. 
    //   need to dequeue a couple of elems
    
    uint16_t halfcount = cq.get_count(&cq)/2;
    uint32_t tmp_sum = 0;
    for (uint16_t i = 0; i < halfcount; i++) {
        cq.dequeue(&cq, buf, &buf_size);
        for (uint16_t j = 0; j < buf_size; j++) {
            tmp_sum += buf[j];
        }
    }
    expc_sum -= tmp_sum;

    spiffs_circular_queue_foreach_dequeue(&cq, buf, buf_size) {
        for (uint16_t i = 0; i < buf_size; i++) {
            real_sum += buf[i];
        };
    }

    assert_equal(expc_sum, real_sum, "SPIFFS Dequeue Non-Empty Foreach. Enqueued elements until full, dequeue half queue, enqueue again until full, and check checksum.");
    printf("        Expected/Real (%d/%d)\n", expc_sum, real_sum);
}

void spiffs_size_variable(void) {
    uint8_t buf[SPIFFS_FULL_QUEUE_ELEM_SIZE+1];
    uint16_t enqueued_bytes = 0;

    _makeseq(SPIFFS_FULL_QUEUE_ELEM_SIZE, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE+1);
    
    while (cq.available_space(&cq) >= SPIFFS_FULL_QUEUE_ELEM_SIZE) {
        cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);
        enqueued_bytes += SPIFFS_FULL_QUEUE_ELEM_SIZE;
    }

    assert_equal(enqueued_bytes, cq.size(&cq), "SPIFFS size function. Enqueueing elems until full, counting enqueued bytes. Compare with size function.");
}

void spiffs_available_space_variable(void) {
    uint8_t buf[SPIFFS_FULL_QUEUE_ELEM_SIZE+1];

    _makeseq(SPIFFS_FULL_QUEUE_ELEM_SIZE, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE+1);
    
    cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);
    uint32_t expected_available_space = cq.max_size - (SPIFFS_FULL_QUEUE_ELEM_SIZE + sizeof(uint16_t)) - \
                                        sizeof(uint16_t);

    assert_equal(expected_available_space, cq.available_space(&cq), "SPIFFS available_space function.");
    printf("      Expected/Read (%d/%d)\n", expected_available_space, cq.available_space(&cq));
}

void spiffs_get_count_variable(void) {
    uint8_t buf[SPIFFS_FULL_QUEUE_ELEM_SIZE+1];

    _makeseq(SPIFFS_FULL_QUEUE_ELEM_SIZE, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE+1);
    
    cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);
    cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);
    cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);

    assert_equal(3, cq.get_count(&cq), "SPIFFS get_count function. Enqueue 3 elems, then check with get_count.");
}

void spiffs_front_variable(void) {
    uint8_t buf[SPIFFS_FULL_QUEUE_ELEM_SIZE+1];
    uint8_t fbuf[SPIFFS_FULL_QUEUE_ELEM_SIZE+1];
    uint16_t fbuf_size = 0;

    _makeseq(SPIFFS_FULL_QUEUE_ELEM_SIZE, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE+1);
    
    cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);
    cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);
    cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);

    memset(fbuf, 0x0, sizeof(fbuf));
    cq.front(&cq, fbuf, &fbuf_size);

    assert_equal(0, memcmp(buf, fbuf, SPIFFS_FULL_QUEUE_ELEM_SIZE), "SPIFFS front function. Enqueue 3 elems, then do memcmp with expected first elem.");
}

void spiffs_is_empty_variable(void) {
    assert_equal(1, cq.is_empty(&cq), "SPIFFS is_empty function. Check on a recently initialized queue.");
}

void spiffs_full_queue_variable(void) {
    uint8_t buf[SPIFFS_FULL_QUEUE_ELEM_SIZE+1];

    _makeseq(SPIFFS_FULL_QUEUE_ELEM_SIZE, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE+1);
    
    while (cq.available_space(&cq) >= SPIFFS_FULL_QUEUE_ELEM_SIZE) {
        cq.enqueue(&cq, buf, SPIFFS_FULL_QUEUE_ELEM_SIZE);
    }

    assert_equal(1, !cq.is_empty(&cq) && cq.available_space(&cq) < SPIFFS_FULL_QUEUE_ELEM_SIZE, "SPIFFS Full Queue. Filling the queue until it's full.");
}

void spiffs_make_two_queues_variable(void) {
    circular_queue_t cq1;
    snprintf(cq1.fn, SPIFFS_FILE_NAME_MAX_SIZE, "/spiffs/test1");
    cq.max_size = 1024;
    assert_equal(spiffs_circular_queue_init(&cq1), 1, "SPIFFS Make Two Queues. Just checking for two independent queues coexistance.");
    cq1.free(&cq1, 0); // set zero to unmount on tear_down
}


void setup() {
    
}

void loop() {
    printf("Testing Variable Size Queue\n");
    cq.elem_size = 0;

    run_test(spiffs_empty_queue_init_variable);
    delay(500);
    run_test(spiffs_empty_queue_double_init_variable);
    delay(500);
    run_test(spiffs_non_empty_queue_variable);
    delay(500);
    run_test(spiffs_make_two_queues_variable);
    delay(500);
    run_test(spiffs_full_queue_variable);
    delay(500);
    run_test(spiffs_wrap_around_variable);
    delay(500);
    run_test(spiffs_dequeue_empty_variable);
    delay(500);
    run_test(spiffs_dequeue_nonempty_variable);
    delay(500);
    run_test(spiffs_dequeue_nonempty_foreach_variable);
    delay(500);
    run_test(spiffs_size_variable);
    delay(500);
    run_test(spiffs_available_space_variable);
    delay(500);
    run_test(spiffs_dequeue_empty_variable);
    delay(500);
    run_test(spiffs_get_count_variable);
    delay(500);
    run_test(spiffs_front_variable);
    delay(500);
    run_test(spiffs_is_empty_variable);
    delay(500);
    
    // printf("Testing Fixed Size Queue\n");
    // cq.elem_size = sizeof(uint32_t);
    


    printf("\n\n");
}