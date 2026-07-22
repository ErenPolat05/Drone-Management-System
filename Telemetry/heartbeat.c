#include "heartbeat.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Compile-Time Assertions & Security Warnings
// ---------------------------------------------------------------------------

/**
 * @brief Ensures struct layout prevents alignment padding memory leakage.
 */
_Static_assert(sizeof(HeartbeatData) == 8, 
               "HeartbeatData size mismatch: Padding detected!");

// ---------------------------------------------------------------------------
// Internal State Storage
// ---------------------------------------------------------------------------

/**
 * @brief Static memory for Drone ID.
 */
static uint16_t _drone_id = 0U;

// ---------------------------------------------------------------------------
// Helper Functions (Endianness Conversion)
// ---------------------------------------------------------------------------

/**
 * @brief Portable 32-bit byte swap for Network Byte Order (Big-Endian).
 * @details Converts 32-bit integers safely without relying on OS headers.
 */
static inline uint32_t host_to_network_32(uint32_t val)
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return val;
#else
    return (uint32_t)(((val & 0x000000FFU) << 24) |
                      ((val & 0x0000FF00U) << 8)  |
                      ((val & 0x00FF0000U) >> 8)  |
                      ((val & 0xFF000000U) >> 24));
#endif
}

/**
 * @brief Portable 16-bit byte swap for Network Byte Order (Big-Endian).
 * @details Converts 16-bit integers safely without relying on OS headers.
 */
static inline uint16_t host_to_network_16(uint16_t val)
{
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return val;
#else
    return (uint16_t)(((val & 0x00FFU) << 8) |
                      ((val & 0xFF00U) >> 8));
#endif
}

// ---------------------------------------------------------------------------
// Core Functions
// ---------------------------------------------------------------------------

void heartbeat_init(uint16_t drone_id)
{
    // Store assigned drone ID
    _drone_id = drone_id;
}

bool heartbeat_generate(
    HeartbeatData *out_packet,
    uint32_t current_timestamp,
    uint8_t current_state
)
{
    // Prevent null pointer dereference
    if (out_packet == NULL)
    {
        return false;
    }

    // Zeroize struct memory to prevent padding data leakage
    memset(out_packet, 0, sizeof(HeartbeatData));

    // Assign packet fields using strict Network Byte Order (Big-Endian)
    out_packet->timestamp = host_to_network_32(current_timestamp);
    out_packet->drone_id  = host_to_network_16(_drone_id);
    out_packet->state     = current_state;
    
    // Explicit defensive programming for reserved space
    out_packet->reserved  = 0U;

    return true;
}