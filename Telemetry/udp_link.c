// udp_link.c
#include "udp_link.h"
#include <string.h>
#include <time.h>

// Cross-Platform Networking Headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET sock_t;
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCK_ERR SOCKET_ERROR
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/time.h>
    typedef int sock_t;
    #define INVALID_SOCK (-1)
    #define SOCK_ERR (-1)
#endif

// ---------------------------------------------------------------------------
// Network State Storage
// ---------------------------------------------------------------------------
static sock_t udp_socket = INVALID_SOCK;
static struct sockaddr_in target_addr;
static uint32_t current_session_id;
static uint32_t current_sequence_id = 0;

// ---------------------------------------------------------------------------
// Helper: cross-platform millisecond-resolution clock for session id
// ---------------------------------------------------------------------------

static uint64_t get_time_ms(void)
{
#ifdef _WIN32
    // Boot-relative tick count, millisecond resolution
    return (uint64_t)GetTickCount64();
#else
    // Wall-clock time with microsecond resolution, converted to milliseconds
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec * 1000ULL) + ((uint64_t)tv.tv_usec / 1000ULL);
#endif
}

// ---------------------------------------------------------------------------
// Core Functions
// ---------------------------------------------------------------------------

bool udp_link_init(const char *ip, uint16_t port) 
{
    // Prevent null pointer dereference before any socket operations
    if (ip == NULL) {
        return false;
    }

    // Reinitialization safety: close any previously open socket first
    // to avoid leaking the old file descriptor if init is called twice
    if (udp_socket != INVALID_SOCK) {
        udp_link_close();
    }

    // Initialize session ID with a millisecond-resolution timestamp so that
    // a reboot/reconnect within the same second still produces a unique ID,
    // preventing replay-detection collisions on the receiver side
    current_session_id = (uint32_t)(get_time_ms() & 0xFFFFFFFFULL);

    // Start every new session with a clean sequence space, paired
    // with the new session_id, so replay-detection stays consistent
    current_sequence_id = 0;

    // Create IPv4 UDP socket
    // NOTE: On Windows, the caller (main.c) is responsible for calling
    // WSAStartup() before this function and WSACleanup() at shutdown.
    // This module no longer owns the WSA lifecycle, so it cannot
    // accidentally tear down other sockets (e.g. TCP) in the process.
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket == INVALID_SOCK) {
        return false;
    }

    // Configure strictly to Non-Blocking mode with proper resource cleanup
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(udp_socket, FIONBIO, &mode) != 0) {
        closesocket(udp_socket);
        udp_socket = INVALID_SOCK;
        return false;
    }
#else
    int flags = fcntl(udp_socket, F_GETFL, 0);
    if (flags == -1) {
        close(udp_socket);
        udp_socket = INVALID_SOCK;
        return false;
    }
    if (fcntl(udp_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(udp_socket);
        udp_socket = INVALID_SOCK;
        return false;
    }
#endif

    // Prepare target address structure
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);

    // Convert literal IPv4 string to binary format (No DNS resolution allowed)
    if (inet_pton(AF_INET, ip, &target_addr.sin_addr) != 1) {
#ifdef _WIN32
        closesocket(udp_socket);
#else
        close(udp_socket);
#endif
        udp_socket = INVALID_SOCK;
        return false;
    }

    return true;
}


/**
 * @brief Formats the header and transmits the payload over the network.
 * * @details Operational Flow:
 * -> Check   : Fails if socket is invalid, payload is oversized, or pointer is NULL.
 * -> Header  : Converts all multi-byte header fields to Big-Endian (Network Byte Order).
 * -> Memory  : Allocates a fixed-size buffer on the stack (Zero VLA usage).
 * -> Combine : Copies header and payload into the transmission buffer.
 * -> Send    : Fires the packet to the cached target address (Fire-and-Forget).
 * -> State   : Increments the sequence ID only upon successful socket handover.
 */
bool udp_link_send(PacketType type, const uint8_t *payload, size_t payload_len) 
{
    // Fail immediately if socket is closed or payload is oversized
    if (udp_socket == INVALID_SOCK || payload_len > UDP_MAX_PAYLOAD_SIZE) {
        return false;
    }

    // Reject invalid API usage: non-zero length with a NULL payload pointer
    if (payload == NULL && payload_len > 0) {
        return false;
    }

    // Format the network header using strict Big-Endian (Network Byte Order)
    TelemetryHeader header;
    header.magic = htons(0xABCD);
    header.version = 1; 
    header.packet_type = (uint8_t)type;
    header.session_id = htonl(current_session_id);
    header.sequence_id = htonl(current_sequence_id);
    header.payload_len = htons((uint16_t)payload_len);

    // Fixed-size buffer array allocation on the stack (No VLA)
    uint8_t tx_buffer[UDP_MAX_PACKET_SIZE];
    
    // Construct the packet layout: [Header] + [Payload]
    memcpy(tx_buffer, &header, sizeof(TelemetryHeader));
    if (payload != NULL && payload_len > 0) {
        memcpy(tx_buffer + sizeof(TelemetryHeader), payload, payload_len);
    }

    size_t total_len = sizeof(TelemetryHeader) + payload_len;

    // Fire-and-Forget transmission utilizing pre-cached target_addr
    int ret = sendto(udp_socket, (const char *)tx_buffer, (int)total_len, 0,
                     (struct sockaddr *)&target_addr, sizeof(target_addr));

    // Handle network congestion/errors silently (EAGAIN, EWOULDBLOCK, ENETUNREACH)
    if (ret == SOCK_ERR) {
        return false; 
    }

    // Increment sequence only on successful transmission to network stack
    current_sequence_id++;
    
    return true;
}


/**
 * @brief Safely closes the UDP socket and resets the internal state.
 * * @details Operational Flow:
 * -> Cleanup : Closes the active file descriptor using the platform-specific API.
 * -> State   : Resets the socket variable to INVALID_SOCK.
 * -> Note    : Windows WSA startup and cleanup are managed externally.
 */
void udp_link_close(void) 
{
    if (udp_socket != INVALID_SOCK) {
#ifdef _WIN32
        closesocket(udp_socket);
#else
        close(udp_socket);
#endif
        udp_socket = INVALID_SOCK;
    }
    // WSA lifecycle (WSAStartup/WSACleanup) is owned by main.c, not this module.
}