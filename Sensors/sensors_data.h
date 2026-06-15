#ifndef SENSORS_DATA_H
#define SENSORS_DATA_H

#include <stdint.h>

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