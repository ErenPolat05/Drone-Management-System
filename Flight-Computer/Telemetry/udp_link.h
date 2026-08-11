// udp_link.h
#ifndef UDP_LINK_H
#define UDP_LINK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Defines the type of data payload being transmitted.
 */
typedef enum {
    PACKET_TYPE_TELEMETRY = 1,
    PACKET_TYPE_HEARTBEAT = 2
} PacketType;

/**
 * @brief Memory limits for the transmission buffer to prevent dynamic allocations.
 */
#define UDP_MAX_PAYLOAD_SIZE 512
#define UDP_MAX_PACKET_SIZE  (sizeof(TelemetryHeader) + UDP_MAX_PAYLOAD_SIZE)

/**
 * @brief Core header attached to every UDP transmission. 
 * Must be packed to prevent alignment padding leakage and ensure network compatibility.
 */

#pragma pack(push,1)
typedef struct  {
    uint16_t magic;         // Validation identifier (0xABCD)
    uint8_t  version;       // Protocol version
    uint8_t  packet_type;   // Enum identifier for payload processing
    uint32_t session_id;    // Unique timestamp generated at init to track reboots
    uint32_t sequence_id;   // Incrementing counter for packet loss detection
    uint16_t payload_len;   // Exact length of the appended data payload
} TelemetryHeader;
#pragma pack(pop)

// Compile-time layout verification
_Static_assert(sizeof(TelemetryHeader) == 14, "TelemetryHeader layout changed");


/**
 * @brief Initializes the UDP socket in non-blocking mode and parses the target IPv4.
 * @param ip Target IPv4 address as a literal string (e.g., "192.168.1.100").
 * @param port Target UDP port.
 * @return true on success, false if socket creation or IP parsing fails.
 *
 * Windows:
 * Caller must initialize Winsock with WSAStartup()
 * before calling udp_link_init().
 */
bool udp_link_init(const char *ip, uint16_t port);

/**
 * @brief Packages and transmits the payload using a Fire-and-Forget architecture.
 * @param type Defines the packet enum identifier.
 * @param payload Pointer to the raw data buffer.
 * @param payload_len Size of the payload (must not exceed UDP_MAX_PAYLOAD_SIZE).
 * @return true if passed to the network stack, false on overflow or socket block (EAGAIN/EWOULDBLOCK).
 */
bool udp_link_send(PacketType type, const uint8_t *payload, size_t payload_len);

/**
 * @brief Closes the socket and releases network resources safely.
 *
 * Windows:
 * Caller is responsible for calling WSACleanup()
 * after the application no longer needs Winsock.
 */
void udp_link_close(void);

#endif // UDP_LINK_H