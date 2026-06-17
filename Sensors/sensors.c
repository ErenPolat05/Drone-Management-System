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