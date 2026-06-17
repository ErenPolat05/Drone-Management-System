#ifndef SENSORS_DATA_H
#define SENSORS_DATA_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// UAV TELEMETRY STATUS AND ERROR FLAGS (16-BIT BITMASKING)
// ---------------------------------------------------------------------------
#define STATUS_OK              0      // 0000 0000 0000 0000 -> All systems are operating normally.

// --- POWER & THERMAL ALARMS ---
#define ERR_BATT_LOW           1      // 0000 0000 0000 0001 -> Battery capacity is below 20%.
#define ERR_BATT_CRIT          2      // 0000 0000 0000 0010 -> Battery capacity is critically low (< 5%). Emergency landing required.
#define ERR_MOTOR_HOT          4      // 0000 0000 0000 0100 -> Motor temperature exceeds safe operating limits (> 80°C).
#define ERR_MOTOR_CRIT         8      // 0000 0000 0000 1000 -> Motor temperature is critical (> 90°C). Risk of fire/failure.
#define ERR_BATT_COLD          16     // 0000 0000 0001 0000 -> Battery temperature is critically low (< 0°C). Risk of voltage drop.
#define ERR_BATT_HOT           32     // 0000 0000 0010 0000 -> Battery temperature is critically high (> 60°C). Risk of thermal runaway.

// --- FLIGHT DYNAMICS ALARMS ---
#define ERR_AERODYNAMICS       64     // 0000 0000 0100 0000 -> Loss of aerodynamic stability (Pitch/Roll angle > 60°).
#define ERR_ALTITUDE_MAX       128    // 0000 0000 1000 0000 -> Maximum legal flight altitude exceeded.
#define ERR_TERRAIN_WARN       256    // 0000 0001 0000 0000 -> Ground proximity warning (Altitude < 5m). Crash risk.

// --- ANOMALY & SECURITY ALARMS ---
#define ERR_FREEFALL           512    // 0000 0010 0000 0000 -> Abnormal altitude drop detected (Freefall anomaly).
#define ERR_GPS_SPOOFING       1024   // 0000 0100 0000 0000 -> Sudden illogical location jump detected (Possible GPS Spoofing).

// NOTE: Bits 2048 to 32768 are reserved for future expansions.
// ---------------------------------------------------------------------------

/**
 * @brief Optimized data structure for holding drone telemetry and sensor data.
 * * Elements are sorted from the largest memory alignment requirement (4 bytes) 
 * to the smallest (1 byte) to prevent the compiler from inserting internal padding.
 */
typedef struct {
    /* 4-Byte Aligned Fields (Floats and 32-bit Integers) */
    uint32_t timestamp;         // System timestamp (e.g., in milliseconds or Unix epoch)
    float latitude;             // GPS Latitude coordinate
    float longitude;            // GPS Longitude coordinate
    float altitude;             // GPS Altitude (height above sea level or takeoff point)
    float pitch;                // IMU Pitch angle
    float roll;                 // IMU Roll angle
    float yaw;                  // IMU Yaw angle

    /* 2-Byte Aligned Fields (16-bit Integers) */
    uint16_t drone_id;          // Unique identifier for the drone
    int16_t motor_temp;         // Motor temperature (Scaled by 10 for precision, e.g., Celsius * 10)
    int16_t battery_temp;       // Battery temperature (Scaled by 10 for precision, e.g., Celsius * 10)
    uint16_t status_code;       // System status or error code

    /* 1-Byte Aligned Fields (8-bit Integers) */
    uint8_t battery_percent;    // Battery charge percentage (0 - 100)
    
    /* * Note: The compiler will automatically add 3 bytes of trailing padding at the end 
     * to align the total struct size to a multiple of 4 (Total size: 40 bytes). 
     * No internal padding is generated between the fields.
     */
} DroneData;



#endif // SENSORS_DATA_H