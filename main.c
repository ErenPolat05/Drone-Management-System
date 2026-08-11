/**
 * @brief Main orchestration file for UAV telemetry system.
 * @details Operational Flow:
 * -> Phase 1 : Hardware & Network Initialization.
 * -> Phase 2 : Non-blocking Super Loop (Telemetry @ 50Hz, Heartbeat @ 1Hz).
 * -> Phase 3 : Graceful Shutdown & Resource Cleanup.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "security/security.h"
#include "sensors/sensors_data.h"
#include "sensors/sensors.h"
#include "telemetry/blackbox.h"
#include "telemetry/heartbeat.h"
#include "telemetry/ring_buffer.h"
#include "telemetry/udp_link.h"

// ---------------------------------------------------------------------------
// System Constants & Limits
// ---------------------------------------------------------------------------

/* Size : 12-byte Nonce + DroneData + 16-byte MAC */
#define ENCRYPTED_PACKET_SIZE (12 + sizeof(DroneData) + 16)

/* Timing : Core loop intervals */
#define TELEMETRY_INTERVAL_MS 20ULL              /* Target : 50 Hz update frequency */
#define TELEMETRY_SLIP_THRESHOLD_MS 100ULL       /* Limit  : Max jitter before skipping to prevent burst spam */
#define HEARTBEAT_INTERVAL_MS 1000ULL            /* Target : 1 Hz heartbeat frequency */
#define HEARTBEAT_SLIP_THRESHOLD_MS 2000ULL      /* Limit  : Max jitter for heartbeat */
#define BURST_SEND_LIMIT 5                       /* Limit  : Max backlog packets sent per cycle */

/* Thresholds : Failsafe triggers */
#define FAILSAFE_DATALINK_TIMEOUT_MS 1000ULL     /* Trigger : 1 second without UDP telemetry */
#define FAILSAFE_HEARTBEAT_TIMEOUT_MS 3000ULL    /* Trigger : 3 seconds without UDP heartbeat */
#define FAILSAFE_BLACKBOX_THRESHOLD 10           /* Trigger : 10 consecutive SD card write errors */

// ---------------------------------------------------------------------------
// Global State Storage
// ---------------------------------------------------------------------------

volatile sig_atomic_t g_system_running = 1;
uint8_t g_current_state = 0; /* State : 0 = Normal, 1 = Failsafe */

// ---------------------------------------------------------------------------
// Core Functions
// ---------------------------------------------------------------------------

/**
 * @brief Intercepts SIGINT (Ctrl+C).
 * @details Flow : Sets system flag to 0 for graceful shutdown.
 */
static void sigint_handler(int signum)
{
    (void)signum;
    g_system_running = 0;
}

/**
 * @brief Retrieves cross-platform millisecond time.
 * @details Flow : Uses GetTickCount64 on Windows, gettimeofday on POSIX.
 */
static uint64_t get_system_time_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
    {
        return 0ULL;
    }
    return ((uint64_t)tv.tv_sec * 1000ULL) + ((uint64_t)tv.tv_usec / 1000ULL);
#endif
}

/**
 * @brief Main execution entry point.
 */
int main(void)
{
#ifdef _WIN32
    // Init : Windows Socket API
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return 1;
    }
#endif

    // Track : Initialization states for safe cleanup
    bool status_sensors = false;
    bool status_security = false;
    bool status_blackbox = false;
    bool status_udp = false;

    // Hook : Register shutdown signal
    if (signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        goto shutdown;
    }

    // Init : Sensor hardware
    status_sensors = sensors_init();
    if (!status_sensors)
    {
        goto shutdown;
    }

    // Config : Cryptographic Key & Nonce Generation
    uint8_t dummy_key[16] = {
        0x4F, 0x1A, 0x7B, 0x92, 0xC3, 0x8D, 0xE5, 0x22,
        0xA1, 0x0F, 0x66, 0x39, 0xBB, 0x44, 0xDF, 0x19
    };
    
    uint64_t startup_time = get_system_time_ms();
    uint8_t dummy_nonce[12] = {0};
    memcpy(dummy_nonce, &startup_time, sizeof(startup_time));

    // Init : Security module
    status_security = security_init(dummy_key, dummy_nonce);
    
    // Security : Zeroize the pre-shared key from RAM immediately
    memset(dummy_key, 0, sizeof(dummy_key));

    if (!status_security)
    {
        goto shutdown;
    }

    // Init : Telemetry buffer queue
    ring_buffer_init();

    // Init : SD Card storage
    status_blackbox = blackbox_init("flight_log.bin");
    if (!status_blackbox)
    {
        goto shutdown;
    }

    // Init : Network transmission
    status_udp = udp_link_init("127.0.0.1", 8080);
    if (!status_udp)
    {
        goto shutdown;
    }

    // Init : System heartbeat (ID: 101)
    heartbeat_init(101);

    // Timing : Setup baseline timestamps
    uint64_t current_time = get_system_time_ms();
    uint64_t last_telemetry_time = current_time;
    uint64_t last_heartbeat_time = current_time;
    
    // Track : Auto-recovery monitoring variables
    uint64_t last_successful_telemetry_time = current_time;
    uint64_t last_successful_heartbeat_time = current_time;
    uint16_t fail_count_blackbox = 0;

    // -----------------------------------------------------------------------
    // Super Loop
    // -----------------------------------------------------------------------
    while (g_system_running != 0)
    {
        current_time = get_system_time_ms();

        // -------------------------------------------------------------------
        // Task : Auto-Recovery & Failsafe Evaluation
        // -------------------------------------------------------------------
        if (((current_time - last_successful_telemetry_time) >= FAILSAFE_DATALINK_TIMEOUT_MS) ||
            ((current_time - last_successful_heartbeat_time) >= FAILSAFE_HEARTBEAT_TIMEOUT_MS) ||
            (fail_count_blackbox >= FAILSAFE_BLACKBOX_THRESHOLD))
        {
            // Action : Enter Failsafe Mode
            g_current_state = 1;
        }
        else
        {
            // Action : Return to Normal Mode (Auto-Recovery)
            g_current_state = 0;
        }

        // -------------------------------------------------------------------
        // Task : 50Hz Live Telemetry (20ms Interval)
        // -------------------------------------------------------------------
        if ((current_time - last_telemetry_time) >= TELEMETRY_INTERVAL_MS)
        {
            DroneData drone_data;
            memset(&drone_data, 0, sizeof(DroneData));

            // Phase 1 : Sensor Read
            bool sensor_status = sensors_update(&drone_data);
            if (!sensor_status)
            {
                // Fallback : Skip cycle if sensor fails
                goto update_telemetry_time;
            }

            uint8_t enc_buffer[ENCRYPTED_PACKET_SIZE];
            memset(enc_buffer, 0, sizeof(enc_buffer));

            // Phase 2 : Encryption
            bool encrypt_status = security_encrypt_payload(&drone_data, enc_buffer, sizeof(enc_buffer));
            if (!encrypt_status)
            {
                // Fallback : Skip cycle if encryption fails
                goto update_telemetry_time;
            }

            // Phase 3 : Blackbox (Historical Record)
            bool blackbox_status = blackbox_log(enc_buffer, sizeof(enc_buffer));
            if (!blackbox_status)
            {
                // Track : Increment failure counter for failsafe
                fail_count_blackbox++;
                // Note : Live UDP must not be blocked by SD card failures.
            }
            else
            {
                fail_count_blackbox = 0;
            }

            // Phase 4 : UDP Link (Live Transmission)
            bool udp_status = udp_link_send(PACKET_TYPE_TELEMETRY, enc_buffer, sizeof(enc_buffer));
            if (udp_status)
            {
                // Track : Mark successful transmission for failsafe monitor
                last_successful_telemetry_time = current_time;

                // Phase 5 : Backlog Burst (Network Recovery)
                uint8_t burst_count = 0;
                while (!ring_buffer_is_empty() && burst_count < BURST_SEND_LIMIT)
                {
                    uint8_t backlog_buffer[ENCRYPTED_PACKET_SIZE];
                    size_t backlog_len = 0;
                    memset(backlog_buffer, 0, sizeof(backlog_buffer));

                    bool pop_status = ring_buffer_pop(backlog_buffer, sizeof(backlog_buffer), &backlog_len);
                    if (pop_status && backlog_len > 0 && backlog_len <= ENCRYPTED_PACKET_SIZE)
                    {
                        bool backlog_udp_status = udp_link_send(PACKET_TYPE_TELEMETRY, backlog_buffer, backlog_len);
                        if (!backlog_udp_status)
                        {
                            // Fallback : Repush packet if network drops again during burst
                            bool repush_status = ring_buffer_push(backlog_buffer, backlog_len);
                            if (!repush_status)
                            {
                                // Note : Buffer full. Packet dropped to prevent lockup.
                            }
                            break;
                        }
                    }
                    burst_count++;
                }
            }
            else
            {
                // Fallback : Buffer live packet if UDP transmission fails
                bool push_status = ring_buffer_push(enc_buffer, sizeof(enc_buffer));
                if (!push_status)
                {
                    // Note : Buffer full. Live packet dropped, system continues.
                }
            }

update_telemetry_time:
            // Sync : Update schedule using Slip Threshold to prevent I/O spam
            if ((current_time - last_telemetry_time) > TELEMETRY_SLIP_THRESHOLD_MS)
            {
                last_telemetry_time = current_time;
            }
            else
            {
                last_telemetry_time += TELEMETRY_INTERVAL_MS;
            }
        }

        // -------------------------------------------------------------------
        // Task : 1Hz System Heartbeat (1000ms Interval)
        // -------------------------------------------------------------------
        if ((current_time - last_heartbeat_time) >= HEARTBEAT_INTERVAL_MS)
        {
            HeartbeatData hb_data;
            memset(&hb_data, 0, sizeof(HeartbeatData));

            // Phase 1 : Generate Status Payload
            bool heartbeat_status = heartbeat_generate(&hb_data, (uint32_t)current_time, g_current_state);
            if (heartbeat_status)
            {
                // Phase 2 : Transmit
                bool hb_udp_status = udp_link_send(PACKET_TYPE_HEARTBEAT, (const uint8_t *)&hb_data, sizeof(HeartbeatData));
                if (!hb_udp_status)
                {
                    // Note : Heartbeat failed. Will retry next 1Hz cycle.
                }
                else
                {
                    // Track : Mark successful transmission for failsafe monitor
                    last_successful_heartbeat_time = current_time;
                }
            }
            
            // Sync : Update schedule
            if ((current_time - last_heartbeat_time) > HEARTBEAT_SLIP_THRESHOLD_MS)
            {
                last_heartbeat_time = current_time;
            }
            else
            {
                last_heartbeat_time += HEARTBEAT_INTERVAL_MS;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Task : Graceful Shutdown Sequence
    // -----------------------------------------------------------------------
shutdown:
    if (status_blackbox)
    {
        blackbox_flush();
        blackbox_close();
    }

    if (status_udp)
    {
        udp_link_close();
    }

    if (status_security)
    {
        security_close();
    }

    if (status_sensors)
    {
        sensors_cleanup();
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}