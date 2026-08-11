#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Memory limits for the circular buffer queue.
 */
#define RING_BUFFER_CAPACITY 100
#define RING_BUFFER_MAX_PAYLOAD 128

/**
 * @brief Initializes the ring buffer state.
 * * @details Operational Flow:
 * -> Reset : Sets head, tail, and count indices to 0.
 */
void ring_buffer_init(void);


/**
 * @brief Pushes new telemetry data into the buffer.
 * * @details Operational Flow:
 * -> Validate  : Rejects payload if length is 0 (DoS protection) or exceeds RING_BUFFER_MAX_PAYLOAD.
 * -> Copy      : Stores the data and its length at the current head index.
 * -> Advance   : Moves the head forward circularly.
 * -> Overwrite : If full, moves the tail forward to overwrite the oldest data.
 */
bool ring_buffer_push(const uint8_t *data, size_t length);


/**
 * @brief Pops the oldest telemetry data from the buffer securely.
 * * @details Operational Flow:
 * -> Check    : Returns false if the buffer is empty.
 * -> Verify   : Returns false if stored data exceeds max_out_length (Buffer Overflow protection).
 * -> Retrieve : Copies the data and length from the tail index to the outputs.
 * -> Advance  : Moves the tail forward circularly and decreases the item count.
 */
bool ring_buffer_pop(uint8_t *out_data, size_t max_out_length, size_t *out_length);

/**
 * @brief Checks if the buffer has any stored items.
 * * @details Operational Flow:
 * -> Check : Returns true if the stored item count is 0.
 */
bool ring_buffer_is_empty(void);

#endif