//includes for ring_buffer.c
#include "ring_buffer.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Internal Data Structures & Storage
// ---------------------------------------------------------------------------

typedef struct {
    uint8_t data[RING_BUFFER_MAX_PAYLOAD];
    size_t length;
} RingBufferItem;

// Static memory allocation to prevent dynamic memory (malloc) usage
static RingBufferItem buffer[RING_BUFFER_CAPACITY];

// Volatile keywords added to prevent compiler optimization errors on shared variables
static volatile size_t head = 0;
static volatile size_t tail = 0;
static volatile size_t count = 0;

// ---------------------------------------------------------------------------
// Core Functions
// ---------------------------------------------------------------------------

void ring_buffer_init(void)
{
    // TODO: ENTER CRITICAL SECTION (Disable Interrupts / Lock Mutex)
    head = 0;
    tail = 0;
    count = 0;
    // TODO: EXIT CRITICAL SECTION (Enable Interrupts / Unlock Mutex)
}

bool ring_buffer_push(const uint8_t *data, size_t length)
{
    // Prevent buffer overflow and Zero-Length DoS attacks
    if (data == NULL || length == 0 || length > RING_BUFFER_MAX_PAYLOAD) {
        return false;
    }

    // TODO: ENTER CRITICAL SECTION (Disable Interrupts / Lock Mutex)
    
    // Copy data and length into the current head position
    memcpy(buffer[head].data, data, length);
    buffer[head].length = length;

    // Advance the write index circularly
    head = (head + 1) % RING_BUFFER_CAPACITY;

    // Handle capacity and overwrite logic
    if (count == RING_BUFFER_CAPACITY) {
        // Buffer is full. Advance tail to silently overwrite the oldest unread packet
        tail = (tail + 1) % RING_BUFFER_CAPACITY;
    } else {
        // Buffer has space. Increment the active item count
        count++;
    }

    // TODO: EXIT CRITICAL SECTION (Enable Interrupts / Unlock Mutex)

    return true;
}

bool ring_buffer_pop(uint8_t *out_data, size_t max_out_length, size_t *out_length)
{
    // Prevent null pointer dereferences
    if (out_data == NULL || out_length == NULL) {
        return false;
    }

    // TODO: ENTER CRITICAL SECTION (Disable Interrupts / Lock Mutex)

    // Prevent reading from an empty buffer
    if (count == 0) {
        // TODO: EXIT CRITICAL SECTION (Enable Interrupts / Unlock Mutex)
        return false;
    }

    // Prevent Stack Smashing / Blind Copy Buffer Overflows on the caller side
    if (buffer[tail].length > max_out_length) {
        // Head-of-Line Blocking'i önlemek için paketi çöpe at (Drop the packet)
        tail = (tail + 1) % RING_BUFFER_CAPACITY;
        count--;
        // TODO: EXIT CRITICAL SECTION (Enable Interrupts / Unlock Mutex)
        return false;
    }

    // Extract data and length from the current tail position
    memcpy(out_data, buffer[tail].data, buffer[tail].length);
    *out_length = buffer[tail].length;

    // Advance the read index circularly
    tail = (tail + 1) % RING_BUFFER_CAPACITY;

    // Decrement the active item count
    count--;

    // TODO: EXIT CRITICAL SECTION (Enable Interrupts / Unlock Mutex)

    return true;
}

bool ring_buffer_is_empty(void)
{
    bool empty_status;
    
    // TODO: ENTER CRITICAL SECTION (Disable Interrupts / Lock Mutex)
    empty_status = (count == 0);
    // TODO: EXIT CRITICAL SECTION (Enable Interrupts / Unlock Mutex)
    
    return empty_status;
}