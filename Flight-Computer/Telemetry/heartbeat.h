#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Defines the fixed-size heartbeat payload.
 * @details Memory Layout:
 * -> Size : Exactly 8 bytes.
 * -> Goal : Indicates system alive status.
 */
typedef struct
{
    uint32_t timestamp;
    uint16_t drone_id;
    uint8_t state;
    uint8_t reserved;
} HeartbeatData;

/**
 * @brief Initializes the heartbeat module.
 * @details Operational Flow:
 * -> Store : Saves the drone ID into internal static memory.
 */
void heartbeat_init(uint16_t drone_id);

/**
 * @brief Generates a secure heartbeat packet in Network Byte Order.
 * @details Operational Flow:
 * -> Check  : Returns false if out_packet is NULL.
 * -> Secure : Clears memory with memset to prevent padding data leakage.
 * -> Endian : Converts timestamp and drone ID to Big-Endian format.
 * -> Assign : Writes fields and explicitly zeroes the reserved byte.
 * -> Return : Returns true on success.
 */
bool heartbeat_generate(
    HeartbeatData *out_packet,
    uint32_t current_timestamp,
    uint8_t current_state
);

#endif