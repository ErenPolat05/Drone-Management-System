#include "sensors.h"
#include "sensors_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>     
#include <math.h>   

// Module-specific file pointer, isolated from the outside world
static FILE *flight_data_file = NULL;

// ---------------------------------------------------------------------------
// Physical boundary constants
// ---------------------------------------------------------------------------
#define LATITUDE_MIN      (-90.0f)
#define LATITUDE_MAX       (90.0f)
#define LONGITUDE_MIN    (-180.0f)
#define LONGITUDE_MAX     (180.0f)
#define ALTITUDE_MIN        (0.0f)
#define ALTITUDE_MAX    (10000.0f)
#define PITCH_MIN        (-180.0f)
#define PITCH_MAX         (180.0f)
#define ROLL_MIN         (-180.0f)
#define ROLL_MAX          (180.0f)
#define YAW_MIN          (-360.0f)
#define YAW_MAX           (360.0f)
#define MOTOR_TEMP_MIN    (-40)
#define MOTOR_TEMP_MAX    (200)
#define BATTERY_TEMP_MIN  (-20)
#define BATTERY_TEMP_MAX  (80)
#define BATTERY_PCT_MAX   (100u)

// ---------------------------------------------------------------------------
// Helper macros — simplify repetitive errno and range checks
// ---------------------------------------------------------------------------

/**
 * @brief Validates overflow and endptr after strtoul.
 * Executes goto error_cleanup on failure.
 */
#define PARSE_ULONG(dst, p, endptr, expected_next)          \
    do {                                                     \
        errno = 0;                                           \
        (dst) = strtoul((p), &(endptr), 10);                 \
        if ((p) == (endptr) || errno == ERANGE               \
            || *(endptr) != (expected_next))                 \
            goto error_cleanup;                              \
        (p) = (endptr) + 1;                                  \
    } while (0)

/**
 * @brief Validates overflow and endptr after strtol.
 */
#define PARSE_LONG(dst, p, endptr, expected_next)           \
    do {                                                     \
        errno = 0;                                           \
        (dst) = strtol((p), &(endptr), 10);                  \
        if ((p) == (endptr) || errno == ERANGE               \
            || *(endptr) != (expected_next))                 \
            goto error_cleanup;                              \
        (p) = (endptr) + 1;                                  \
    } while (0)

/**
 * @brief Validates NaN/Inf and endptr after strtof.
 */
#define PARSE_FLOAT(dst, p, endptr, expected_next)          \
    do {                                                     \
        errno = 0;                                           \
        (dst) = strtof((p), &(endptr));                      \
        if ((p) == (endptr) || errno == ERANGE               \
            || isnan(dst) || isinf(dst)                      \
            || *(endptr) != (expected_next))                 \
            goto error_cleanup;                              \
        (p) = (endptr) + 1;                                  \
    } while (0)

/**
 * @brief Physical range check — triggers goto error_cleanup if out of bounds.
 */
#define RANGE_CHECK(val, lo, hi)                            \
    do {                                                     \
        if ((val) < (lo) || (val) > (hi))                    \
            goto error_cleanup;                              \
    } while (0)

// ---------------------------------------------------------------------------

/**
 * @brief Safely reads a single CSV line and parses it into a TEMPORARY struct;
 * performs an atomic copy to the main drone_ptr only if all steps succeed.
 *
 * @param drone_ptr  Target DroneData pointer.
 * @return true  -> success, false -> EOF or any error.
 */

static bool read_and_parse_line(DroneData *drone_ptr)
{
    // Verify that the file pointer is valid upon entering the function.
    if (flight_data_file == NULL) {
        return false;
    }

    char line_buffer[128];

    // 1. Safe line reading
    if (fgets(line_buffer, sizeof(line_buffer), flight_data_file) == NULL) {
        goto error_cleanup;
    }

    // 2. Partial line protection
    if (strchr(line_buffer, '\n') == NULL && !feof(flight_data_file)) {
        goto error_cleanup;
    }

    //Temporary struct — data is not written to main memory until all fields are verified.
    DroneData tmp = {0};

    char *p = line_buffer;
    char *endptr;
    unsigned long ul_tmp;
    long l_tmp;

    // --- drone_id (uint16_t) ---
    // errno is reset, ERANGE is caught after overflow.
    PARSE_ULONG(ul_tmp, p, endptr, ',');
    if (ul_tmp > UINT16_MAX) goto error_cleanup;
    tmp.drone_id = (uint16_t)ul_tmp;

    // --- timestamp (uint32_t) ---
    PARSE_ULONG(ul_tmp, p, endptr, ',');
    if (ul_tmp > UINT32_MAX) goto error_cleanup;
    tmp.timestamp = (uint32_t)ul_tmp;

    // --- latitude (float) ---
    // FIX #3: isnan / isinf; FIX #4: physical range
    PARSE_FLOAT(tmp.latitude, p, endptr, ',');
    RANGE_CHECK(tmp.latitude, LATITUDE_MIN, LATITUDE_MAX);

    // --- longitude (float) ---
    PARSE_FLOAT(tmp.longitude, p, endptr, ',');
    RANGE_CHECK(tmp.longitude, LONGITUDE_MIN, LONGITUDE_MAX);

    // --- altitude (float) ---
    PARSE_FLOAT(tmp.altitude, p, endptr, ',');
    RANGE_CHECK(tmp.altitude, ALTITUDE_MIN, ALTITUDE_MAX);

    // --- pitch (float) ---
    PARSE_FLOAT(tmp.pitch, p, endptr, ',');
    RANGE_CHECK(tmp.pitch, PITCH_MIN, PITCH_MAX);

    // --- roll (float) ---
    PARSE_FLOAT(tmp.roll, p, endptr, ',');
    RANGE_CHECK(tmp.roll, ROLL_MIN, ROLL_MAX);

    // --- yaw (float) ---
    PARSE_FLOAT(tmp.yaw, p, endptr, ',');
    RANGE_CHECK(tmp.yaw, YAW_MIN, YAW_MAX);

    // --- motor_temp (int16_t) ---
    PARSE_LONG(l_tmp, p, endptr, ',');
    if (l_tmp < INT16_MIN || l_tmp > INT16_MAX) goto error_cleanup;
    RANGE_CHECK((int)l_tmp, MOTOR_TEMP_MIN, MOTOR_TEMP_MAX);
    tmp.motor_temp = (int16_t)l_tmp;

    // --- battery_temp (int16_t) ---
    PARSE_LONG(l_tmp, p, endptr, ',');
    if (l_tmp < INT16_MIN || l_tmp > INT16_MAX) goto error_cleanup;
    RANGE_CHECK((int)l_tmp, BATTERY_TEMP_MIN, BATTERY_TEMP_MAX);
    tmp.battery_temp = (int16_t)l_tmp;

    // --- battery_percent (uint8_t) — final field, ends with \n / \r / \0 ---
    // Manual parsing is used instead of the macro because the final field does not end with a comma.
    errno = 0;
    ul_tmp = strtoul(p, &endptr, 10);
    if (p == endptr || errno == ERANGE
        || (*endptr != '\n' && *endptr != '\r' && *endptr != '\0')) {
        goto error_cleanup;
    }
    if (ul_tmp > BATTERY_PCT_MAX) goto error_cleanup;
    tmp.battery_percent = (uint8_t)ul_tmp;

    // FIX #5: Atomic update — the main struct is written only if execution reaches here.
    *drone_ptr = tmp;
    return true;

error_cleanup:
    if (flight_data_file != NULL) {
        fclose(flight_data_file);
        flight_data_file = NULL; // Use-After-Free protection
    }
    return false;
}
// ---------------------------------------------------------------------------

/**
 * @brief Comprehensive telemetry diagnostic suite for real-time UAV monitoring.
 * * This section contains static helper functions responsible for the health, 
 * safety, and security of the drone. The diagnostic checks are divided into three parts:
 * 1. Power & Thermal: Monitors battery charge levels and hardware temperatures.
 * 2. Flight Dynamics: Ensures aerodynamic stability and compliance with legal altitude limits.
 * 3. Security & Anomalies: Detects physical failures (freefall) and cyber threats 
 * (GPS spoofing) using time-based kinematic calculations.
 * * All functions utilize Bitwise OR (|=) operations to safely stack exclusive 
 * error flags into the drone's status_code without overwriting each other.
 */

static void check_battery_and_temp(DroneData *drone_ptr)
{
   // Battery checks: If critical, do NOT set the low warning.
   if(drone_ptr->battery_percent < 5){
    drone_ptr->status_code |= ERR_BATT_CRIT;
   }else if(drone_ptr->battery_percent < 20){
    drone_ptr->status_code |= ERR_BATT_LOW;
   }

   // Motor temp checks: If critical, do NOT set the hot warning.
   if(drone_ptr->motor_temp > 90){
    drone_ptr->status_code |= ERR_MOTOR_CRIT;
   }else if(drone_ptr->motor_temp > 80){
    drone_ptr->status_code |= ERR_MOTOR_HOT;
   }

   // Battery temp check: Covers both freezing and overheating (Li-Po safety limits)
   if(drone_ptr->battery_temp < 0){
    drone_ptr->status_code |= ERR_BATT_COLD;
   }else if(drone_ptr->battery_temp >60){
    drone_ptr->status_code |= ERR_BATT_HOT;
   }
}

static void check_flight_dynamics(DroneData *drone_ptr)
{
  // Aerodynamics check: Loss of balance if pitch or roll exceeds 60 degrees in any direction
  if(drone_ptr->pitch > 60.0f || drone_ptr->pitch < -60.0f ||
     drone_ptr->roll  > 60.0f || drone_ptr->roll  < -60.0f){
        drone_ptr->status_code |= ERR_AERODYNAMICS;
     }


  // Altitude check: Breaching the legal flight ceiling of 120 meters
  if(drone_ptr->altitude > 120.0f){
    drone_ptr->status_code |= ERR_ALTITUDE_MAX;
  }else if(drone_ptr->altitude <  5.0f){
    drone_ptr->status_code |= ERR_TERRAIN_WARN;
  }
}

static void check_anomaly_and_spoofing(DroneData *current_data)
{
    static DroneData prev_data;
    static bool is_first_run = true;

    // If this is the first execution, initialize the memory and exit
    if(is_first_run){
        prev_data = *current_data;
        is_first_run = false;
        return;
    }

    // Calculate Delta T in seconds (assuming timestamp is in milliseconds)
    float delta_t = (float)(current_data->timestamp - prev_data.timestamp) / 1000.0f;

    // Prevent Division by Zero just in case the timestamp hasn't updated then return the operation.
    if(delta_t <= 0.0f){
        return;
    }

    // -----------------------------------------------------------------------
    // 1. Freefall Check (Vertical Velocity)
    // Formula: v = (z1 - z2) / dt
    // -----------------------------------------------------------------------
    float drop_velocity = (prev_data.altitude - current_data->altitude) / delta_t;

    // If the drone is falling faster than 20 m/s (approx. 72 km/h), it's in freefall
    if(drop_velocity > 20.0f){
        current_data->status_code |= ERR_FREEFALL;
    }

    // -----------------------------------------------------------------------
    // 2. GPS Spoofing Check (Angular Velocity)
    // Formula: v = |x2 - x1| / dt
    // -----------------------------------------------------------------------
    float lat_diff = prev_data.latitude - current_data->latitude;
    float lon_diff = prev_data.longitude - current_data->longitude;
    
    // Manual absolute value to save CPU cycles (no math.h dependencies like fabs)
    if (lat_diff < 0.0f) lat_diff = -lat_diff;
    if (lon_diff < 0.0f) lon_diff = -lon_diff;
    
    // Manual absolute value to save CPU cycles (no math.h dependencies like fabs)
    float lat_speed = lat_diff / delta_t;
    float lon_speed = lon_diff / delta_t;

    // Tolerance: Max physical speed in degrees per second.
    // 0.0005 deg/sec is approximately 55 m/s (200 km/h). 
    // Anything faster means the drone teleported (Spoofing Attack).
    if(lat_speed > 0.0005f || lon_speed > 0.0005f){
        current_data->status_code |= ERR_GPS_SPOOFING;
    }

    // Update the buffer for the next cycle
    prev_data = *current_data;
}

// ---------------------------------------------------------------------------

bool sensors_init(void)
{
    flight_data_file = fopen("flight_data.csv", "r");
    if (flight_data_file == NULL) {
        return false;
    }

    char header_buffer[128];
    if (fgets(header_buffer, sizeof(header_buffer), flight_data_file) == NULL) {
        fclose(flight_data_file);
        flight_data_file = NULL;
        return false;
    }

    if (strchr(header_buffer, '\n') == NULL && !feof(flight_data_file)) {
        fclose(flight_data_file);
        flight_data_file = NULL;
        return false;
    }

    return true;
}

bool sensors_update(DroneData *drone_ptr)
{
    // Null pointer protection
    if (drone_ptr == NULL) {
        return false;
    }

    // Upper-level check — fgets is never reached with a NULL file pointer.
    if (flight_data_file == NULL) {
        return false;
    }

    // Read and parse the next line of telemetry data
    if (!read_and_parse_line(drone_ptr)) {
        return false;
    }
    
    // Reset status code to STATUS_OK (0) to clear previous cycles' flags
    drone_ptr->status_code = STATUS_OK;

    // Run the health monitor diagnostics to set appropriate exclusive error flags
    check_battery_and_temp(drone_ptr);
     
    // Run the flight dynamics and legal limits diagnostics
    check_flight_dynamics(drone_ptr);

    // Run the cyber security and anomaly detection diagnostics
    check_anomaly_and_spoofing(drone_ptr);

    return true;
}