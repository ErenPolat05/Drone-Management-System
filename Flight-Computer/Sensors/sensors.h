#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include "sensors_data.h"

/**
 * @brief Initializes the sensors.
 * * @return true if initialization is successful, false otherwise.
 */
bool sensors_init(void);

/**
 * @brief Updates the telemetry data inside the provided struct pointer.
 * * @param drone_ptr Pointer to the DroneData struct where the sensor data will be updated.
 * * @return true if the update is successful, false if a read error or data loss occurs.
 */
bool sensors_update(DroneData *drone_ptr);

/**
 * @brief Close the sensors.
 * * @return true if initialization is successful, false otherwise.
 */
bool sensors_cleanup(void);

#endif