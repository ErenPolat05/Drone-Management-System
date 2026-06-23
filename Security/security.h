#ifndef SECURITY_H
#define SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "Sensors/sensors_data.h"

#define NONCE_SIZE 12
#define MAC_SIZE 16

/**
 * @brief Dynamic calculation of TX buffer size to prevent offset vulnerabilities.
 */
#define TX_BUFFER_SIZE (NONCE_SIZE + sizeof(DroneData) + MAC_SIZE)

/*
 * Note: The caller providing the dynamic_key should securely erase its temporary 
 * key material using mbedtls_platform_zeroize() or equivalent after this function returns.
 */
/**
 * @brief Initializes the mbedTLS context, PSK, and starting nonce.
 * @param dynamic_key 16-byte Pre-Shared Key injected dynamically.
 * @param initial_nonce 12-byte starting nonce to prevent nonce reuse.
 * @return true if initialization is successful, false otherwise.
 */
bool security_init(const uint8_t *dynamic_key,
                   const uint8_t *initial_nonce);

/**
 * @brief Encrypts the telemetry payload using AES-128-GCM, appends MAC and Nonce.
 * @param input_data Pointer to the packed DroneData struct.
 * @param tx_buffer Target buffer for the transmission payload.
 * @param buffer_len Size of the target buffer (must be >= TX_BUFFER_SIZE).
 * @return true if encryption and packaging are successful, false on overflow or failure.
 */
bool security_encrypt_payload(const DroneData *input_data,
                               uint8_t *tx_buffer, 
                               size_t buffer_len);

/**
 * @brief Safely wipes the RAM and releases the mbedTLS context.
 * @return true if successfully closed and zeroized.
 */
void security_close(void);

#endif