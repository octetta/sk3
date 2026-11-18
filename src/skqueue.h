#ifndef SKRED_SKQUEUE_H
#define SKRED_SKQUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "portable_atomic.h"
#include "seq.h"

typedef struct {
    uint64_t timestamp;
    uint64_t id;
    int tag;
    void *data;
    event_t event;
    atomic_int_t cancelled;  // 0 = active, 1 = cancelled
    atomic_int_t ready;      // 0 = free/reserved, 1 = fully published
    int generation;
} skqueue_item_t;
typedef skqueue_item_t item_t;

// Lock-free ring buffer for incoming items
typedef struct {
    skqueue_item_t *items;
    atomic_int_t write_idx;
    atomic_int_t read_idx;
    int capacity;
} skqueue_ring_buffer_t;
typedef skqueue_ring_buffer_t ring_buffer_t;

// Priority queue (min-heap) for sorted output
typedef struct {
    skqueue_item_t *heap;
    int size;
    int capacity;
} skqueue_priority_queue_t;
typedef skqueue_priority_queue_t priority_queue_t;

typedef struct {
    skqueue_ring_buffer_t incoming;  // Lock-free MPSC queue
    skqueue_priority_queue_t sorted; // Min-heap for sorted access
    int max_size;
    simple_mutex_t sorted_mutex;
    atomic_int_t generation;
} skqueue_t;
typedef skqueue_t queue_t;

void skqueue_init(skqueue_t *q, int max_size);
void skqueue_free(skqueue_t *q);

// Lock-free: Multiple producers can call this (returns false if full)
bool skqueue_put_event(skqueue_t *q, uint64_t timestamp, int tag, void *data, const event_t *event);

// Non-blocking for audio callback: returns false if admin owns the heap.
bool skqueue_get_filtered(skqueue_t *q, uint64_t limit_ts, skqueue_item_t *out);
bool skqueue_peek_timestamp(skqueue_t *q, uint64_t *timestamp);

int skqueue_size(skqueue_t *q);

// Admin operations: callback receives each item, return 0 to continue, non-zero to stop
typedef int (*skqueue_foreach_cb)(const skqueue_item_t *item, void *userdata);
typedef skqueue_foreach_cb queue_foreach_cb;
void skqueue_foreach(skqueue_t *q, skqueue_foreach_cb callback, void *userdata);

// Cancel items matching a predicate using tombstones.
typedef bool (*skqueue_cancel_cb)(const skqueue_item_t *item, void *userdata);
typedef skqueue_cancel_cb queue_cancel_cb;
int skqueue_cancel(skqueue_t *q, skqueue_cancel_cb should_cancel, void *userdata);

void skqueue_clear(skqueue_t *q);

// Legacy wrappers for backwards compatibility
void queue_init(queue_t *q, int max_size);
void queue_free(queue_t *q);
bool queue_put_event(queue_t *q, uint64_t timestamp, int tag, void *data, const event_t *event);
bool queue_get_filtered(queue_t *q, uint64_t limit_ts, item_t *out);
bool queue_peek_timestamp(queue_t *q, uint64_t *timestamp);
int queue_size(queue_t *q);
void queue_foreach(queue_t *q, queue_foreach_cb callback, void *userdata);
int queue_cancel(queue_t *q, queue_cancel_cb should_cancel, void *userdata);
void queue_clear(queue_t *q);

#endif /* SKRED_SKQUEUE_H */
