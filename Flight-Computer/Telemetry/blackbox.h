#ifndef BLACKBOX_H
#define BLACKBOX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Memory limits for the blackbox file buffer.
 */
#define BLACKBOX_BUFFER_SIZE 4096
#define BLACKBOX_MAX_PAYLOAD_SIZE 128

/**
 * @brief Initializes the blackbox file stream.
 * * @details Operational Flow:
 * -> Check : Returns false if a file is already open.
 * -> Open  : Opens the specified file in binary append mode ("ab").
 * -> Reset : Sets buffer offset to 0.
 */
bool blackbox_init(const char *filepath);

/**
 * @brief Buffers telemetry data to reduce SD card write cycles.
 * * @details Operational Flow:
 * -> Check : Rejects NULL data, zero length, or oversized payloads (DoS protection).
 * -> Flush : Empties the buffer to disk if incoming data exceeds remaining space.
 * -> Safe  : Aborts copying and retains RAM buffer if flush fails.
 * -> Copy  : Appends data to the static memory buffer.
 */
bool blackbox_log(const uint8_t *data, size_t length);

/**
 * @brief Forces the buffered data to be written to the SD card safely.
 * * @details Operational Flow:
 * -> Check : Returns false if file is closed or buffer is empty.
 * -> Write : Uses fwrite to move data from RAM to disk. Validates byte count.
 * -> Sync  : Calls fflush and fsync to force OS and hardware-level disk synchronization.
 * -> Reset : Clears the buffer offset only if all I/O operations succeed.
 */
bool blackbox_flush(void);

/**
 * @brief Safely flushes remaining data and closes the blackbox file.
 * * @details Operational Flow:
 * -> Flush : Writes any lingering data to disk.
 * -> Close : Terminates the file descriptor safely.
 */
void blackbox_close(void);

#endif