#include "blackbox.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h> // Required for fsync() and POSIX I/O operations


#ifdef _WIN32
#include <io.h>
#define fsync _commit
#define fileno _fileno
#endif


// ---------------------------------------------------------------------------
// Internal Data Structures & Storage
// ---------------------------------------------------------------------------

// Static memory allocation to prevent dynamic memory (malloc) usage
static FILE *log_file = NULL;
static uint8_t write_buffer[BLACKBOX_BUFFER_SIZE];
static size_t buffer_offset = 0;

// ---------------------------------------------------------------------------
// Core Functions
// ---------------------------------------------------------------------------

/**
 * @brief Initializes the blackbox file stream.
 * @details Operational Flow:
 * -> Check : Returns false if a file is already open.
 * -> Open  : Opens target file in binary append mode ("ab").
 * -> Reset : Sets buffer offset to 0.
 */
bool blackbox_init(const char *filepath)
{
    // Prevent opening multiple instances or overwriting active file pointers
    if (log_file != NULL) {
        return false;
    }
    
    //check the filepath
    if (filepath == NULL) {
    return false;
    }

    // Open target file in binary append mode ("ab")
    log_file = fopen(filepath, "ab");
    if (log_file == NULL) {
        return false;
    }

    // Reset buffer state
    buffer_offset = 0;

    return true;
}

/**
 * @brief Performs the actual disk writing process without thread locks.
 * @details Operational Flow:
 * -> Check : Aborts if file is closed or buffer is empty.
 * -> Write : Moves RAM data to disk and validates byte count.
 * -> Sync  : Forces OS and hardware-level disk synchronization.
 * -> Reset : Clears buffer offset only on complete success.
 * NOTE: Private helper to prevent self-deadlocks inside locked functions.
 */
static bool _blackbox_flush_unlocked(void)
{
    // Prevent disk operations if the file is closed or the buffer is empty
    if (log_file == NULL || buffer_offset == 0) {
        return false;
    }

    // Write buffered data to the disk and validate the byte count
    size_t written = fwrite(write_buffer, 1, buffer_offset, log_file);
    if (written != buffer_offset) {
        // I/O Error: Abort the operation and keep the RAM buffer intact
        return false;
    }

    // Force the OS stdio subsystem to flush data to the kernel
    if (fflush(log_file) != 0) {
        // I/O Error: Abort the operation and keep the RAM buffer intact
        return false;
    }

    // Hardware Synchronization: Force the kernel to write data to physical storage
    if (fsync(fileno(log_file)) != 0) {
        return false;
    }

    // Reset the buffer counter only after complete success
    buffer_offset = 0;

    return true;
}

/**
 * @brief Buffers telemetry data to reduce SD card write cycles.
 * @details Operational Flow:
 * -> Check : Rejects NULL data, zero length, or oversized payloads (DoS protection).
 * -> Lock  : Enters critical section.
 * -> Flush : Calls unlocked helper if incoming data exceeds remaining space.
 * -> Copy  : Appends data to the static memory buffer.
 * -> Unlock: Exits critical section.
 */
bool blackbox_log(const uint8_t *data, size_t length)
{
    // Storage DoS Protection: Reject invalid lengths and null pointers
    if (data == NULL || length == 0 || length > BLACKBOX_MAX_PAYLOAD_SIZE) {
        return false;
    }

    // TODO: ENTER CRITICAL SECTION (Lock Mutex / Disable Interrupts)

    // Capacity Check: If new data does not fit, flush existing buffer to disk
    if (buffer_offset + length > BLACKBOX_BUFFER_SIZE) {
        // NOTE: Fixed to use _unlocked helper to prevent self-deadlock
        if (!_blackbox_flush_unlocked()) {
            // I/O Error handling: Do not overwrite RAM buffer, abort operation
            // TODO: EXIT CRITICAL SECTION (Unlock Mutex / Enable Interrupts)
            return false;
        }
    }

    // Copy new data into the static buffer
    memcpy(write_buffer + buffer_offset, data, length);
    
    // Advance the offset index
    buffer_offset += length;

    // TODO: EXIT CRITICAL SECTION (Unlock Mutex / Enable Interrupts)

    return true;
}

/**
 * @brief Thread-safe public wrapper for flushing buffered data to the SD card.
 * @details Operational Flow:
 * -> Lock   : Enters critical section.
 * -> Flush  : Delegates disk operations to the unlocked helper.
 * -> Unlock : Exits critical section.
 */
bool blackbox_flush(void)
{
    // TODO: ENTER CRITICAL SECTION (Lock Mutex / Disable Interrupts)

    bool result;

    result = _blackbox_flush_unlocked();

    // TODO: EXIT CRITICAL SECTION (Unlock Mutex / Enable Interrupts)

    return result;
}

/**
 * @brief Safely flushes remaining data and closes the blackbox file.
 * @details Operational Flow:
 * -> Lock   : Enters critical section.
 * -> Flush  : Calls unlocked helper to write lingering data to disk.
 * -> Close  : Terminates the file descriptor safely.
 * -> Unlock : Exits critical section.
 */
void blackbox_close(void)
{
    // TODO: ENTER CRITICAL SECTION (Lock Mutex / Disable Interrupts)

    // Graceful shutdown: Flush remaining data and release file descriptor
    if (log_file != NULL) {
        // NOTE: Fixed to use _unlocked helper to prevent self-deadlock
        _blackbox_flush_unlocked(); 
        fclose(log_file);
        log_file = NULL;
    }

    // TODO: EXIT CRITICAL SECTION (Unlock Mutex / Enable Interrupts)
}