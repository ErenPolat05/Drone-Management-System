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

/* Cryptographic packet size allocation */
#define ENCRYPTED_PACKET_SIZE (12 + sizeof(DroneData) + 16)

/* Timing and Scheduling Constants */
#define TELEMETRY_INTERVAL_MS 20ULL              /* 50 Hz update frequency for main telemetry loop */
#define TELEMETRY_SLIP_THRESHOLD_MS 100ULL       /* Maximum allowed jitter to prevent network burst spam */
#define HEARTBEAT_INTERVAL_MS 1000ULL            /* 1 Hz update frequency for system heartbeat signal */
#define HEARTBEAT_SLIP_THRESHOLD_MS 2000ULL      /* Maximum allowed jitter for heartbeat generation */
#define BURST_SEND_LIMIT 5                       /* Maximum number of backlog packets to transmit at once */

/* Failsafe Thresholds */
#define FAILSAFE_DATALINK_TIMEOUT_MS 1000ULL     /* Time without successful UDP telemetry before failsafe (1 second) */
#define FAILSAFE_HEARTBEAT_TIMEOUT_MS 3000ULL    /* Time without successful UDP heartbeat before failsafe (3 seconds) */
#define FAILSAFE_BLACKBOX_THRESHOLD 10           /* Consecutive SD card write errors before failsafe */

volatile sig_atomic_t g_system_running = 1;
uint8_t g_current_state = 0; /* 0: Normal, 1: Failsafe */

static void sigint_handler(int signum)
{
    (void)signum;
    g_system_running = 0;
}

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

int main(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return 1;
    }
#endif

    bool status_sensors = false;
    bool status_security = false;
    bool status_blackbox = false;
    bool status_udp = false;

    if (signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        goto shutdown;
    }

    status_sensors = sensors_init();
    if (!status_sensors)
    {
        goto shutdown;
    }

    uint8_t dummy_key[16] = {
        0x4F, 0x1A, 0x7B, 0x92, 0xC3, 0x8D, 0xE5, 0x22,
        0xA1, 0x0F, 0x66, 0x39, 0xBB, 0x44, 0xDF, 0x19
    };
    
    uint64_t startup_time = get_system_time_ms();
    uint8_t dummy_nonce[12] = {0};
    memcpy(dummy_nonce, &startup_time, sizeof(startup_time));

    status_security = security_init(dummy_key, dummy_nonce);
    
    /* Key Zeroization: Clear the pre-shared key from RAM immediately after initialization */
    memset(dummy_key, 0, sizeof(dummy_key));

    if (!status_security)
    {
        goto shutdown;
    }

    ring_buffer_init();

    status_blackbox = blackbox_init("flight_log.bin");
    if (!status_blackbox)
    {
        goto shutdown;
    }

    status_udp = udp_link_init("127.0.0.1", 8080);
    if (!status_udp)
    {
        goto shutdown;
    }

    heartbeat_init(101);

    uint64_t current_time = get_system_time_ms();
    uint64_t last_telemetry_time = current_time;
    uint64_t last_heartbeat_time = current_time;
    
    uint64_t last_successful_telemetry_time = current_time;
    uint64_t last_successful_heartbeat_time = current_time;
    uint16_t fail_count_blackbox = 0;

    while (g_system_running != 0)
    {
        current_time = get_system_time_ms();

        /* Failsafe Auto-Recovery System Evaluation */
        if (((current_time - last_successful_telemetry_time) >= FAILSAFE_DATALINK_TIMEOUT_MS) ||
            ((current_time - last_successful_heartbeat_time) >= FAILSAFE_HEARTBEAT_TIMEOUT_MS) ||
            (fail_count_blackbox >= FAILSAFE_BLACKBOX_THRESHOLD))
        {
            g_current_state = 1;
        }
        else
        {
            g_current_state = 0;
        }

        if ((current_time - last_telemetry_time) >= TELEMETRY_INTERVAL_MS)
        {
            DroneData drone_data;
            memset(&drone_data, 0, sizeof(DroneData));

            bool sensor_status = sensors_update(&drone_data);
            if (!sensor_status)
            {
                goto update_telemetry_time;
            }

            uint8_t enc_buffer[ENCRYPTED_PACKET_SIZE];
            memset(enc_buffer, 0, sizeof(enc_buffer));

            bool encrypt_status = security_encrypt_payload(&drone_data, enc_buffer, sizeof(enc_buffer));
            if (!encrypt_status)
            {
                goto update_telemetry_time;
            }

            bool blackbox_status = blackbox_log(enc_buffer, sizeof(enc_buffer));
            if (!blackbox_status)
            {
                fail_count_blackbox++;
                /* Blackbox failed to write historical data this cycle.
                   Live UDP telemetry must not be blocked. Proceeding to UDP transmission. */
            }
            else
            {
                fail_count_blackbox = 0;
            }

            bool udp_status = udp_link_send(PACKET_TYPE_TELEMETRY, enc_buffer, sizeof(enc_buffer));
            if (udp_status)
            {
                last_successful_telemetry_time = current_time;

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
                            bool repush_status = ring_buffer_push(backlog_buffer, backlog_len);
                            if (!repush_status)
                            {
                                /* Ring buffer is full, cannot repush backlog packet.
                                   Packet dropped to prevent system lockup. */
                            }
                            break;
                        }
                    }
                    burst_count++;
                }
            }
            else
            {
                bool push_status = ring_buffer_push(enc_buffer, sizeof(enc_buffer));
                if (!push_status)
                {
                    /* UDP failed and Ring Buffer is full.
                       Packet dropped from live queue, system continues normally. */
                }
            }

update_telemetry_time:
            if ((current_time - last_telemetry_time) > TELEMETRY_SLIP_THRESHOLD_MS)
            {
                last_telemetry_time = current_time;
            }
            else
            {
                last_telemetry_time += TELEMETRY_INTERVAL_MS;
            }
        }

        if ((current_time - last_heartbeat_time) >= HEARTBEAT_INTERVAL_MS)
        {
            HeartbeatData hb_data;
            memset(&hb_data, 0, sizeof(HeartbeatData));

            bool heartbeat_status = heartbeat_generate(&hb_data, (uint32_t)current_time, g_current_state);
            if (heartbeat_status)
            {
                bool hb_udp_status = udp_link_send(PACKET_TYPE_HEARTBEAT, (const uint8_t *)&hb_data, sizeof(HeartbeatData));
                if (!hb_udp_status)
                {
                    /* Heartbeat UDP send failed.
                       System continues; will retry on the next 1Hz cycle. */
                }
                else
                {
                    last_successful_heartbeat_time = current_time;
                }
            }
            
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