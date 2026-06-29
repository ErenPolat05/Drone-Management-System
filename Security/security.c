#include "security.h"
#include "mbedtls/gcm.h"
#include "mbedtls/platform_util.h"
#include <string.h>


// ---------------------------------------------------------------------------
// Cryptographic State Storage
// ---------------------------------------------------------------------------

// Static mbedTLS GCM context
static mbedtls_gcm_context gcm_ctx;

// 12-byte stateful counter (Nonce)
static uint8_t nonce[NONCE_SIZE];


// ---------------------------------------------------------------------------
// Core Functions
// ---------------------------------------------------------------------------

/**
 * @brief Initializes the AES-GCM encryption context and state.
 * * @details Operational Flow:
 * -> Input Validation : Ensures pointers are valid to prevent crashes.
 * -> Context Init     : Prepares the mbedTLS internal structures.
 * -> Nonce Setup      : Copies the external 12-byte starting counter into the static array.
 * -> Key Setup        : Loads the 128-bit dynamic AES key into the context.
 */
bool security_init(const uint8_t *dynamic_key,const uint8_t *initial_nonce)
{

  // Validate injected key and nonce to prevent null pointer dereference
  if(dynamic_key == NULL || initial_nonce == NULL){
    return false;
  }

  mbedtls_gcm_init(&gcm_ctx);

  // Initialize the static nonce array with the provided initial_nonce
  memcpy(nonce,initial_nonce,NONCE_SIZE);

  // Configure the AES-128 key dynamically
  int ret = mbedtls_gcm_setkey(&gcm_ctx,MBEDTLS_CIPHER_ID_AES,dynamic_key,128);
  if(ret != 0){
    mbedtls_platform_zeroize(&gcm_ctx,sizeof(mbedtls_gcm_context));
    return false;
  }

  return true;
}

/**
 * @brief Encrypts the telemetry payload directly into the transmission buffer.
 * * @details Operational Flow:
 * -> Safety Checks : Verifies input validity and ensures the target buffer is large enough (60 bytes).
 * -> Memory Layout : Divides the TX buffer into Nonce (12B) | Ciphertext (32B) | MAC (16B).
 * -> Zero-Copy     : Reads directly from the packed input_data struct memory to save RAM.
 * -> State Update  : Increments the 12-byte nonce (Big-Endian format) for the next transmission.
 */
bool security_encrypt_payload(const DroneData *input_data,uint8_t *tx_buffer,size_t buffer_len)
{
   // Buffer Overflow and Null Pointer protections
   if(input_data == NULL || tx_buffer == NULL || buffer_len < TX_BUFFER_SIZE){
    return false;
   }

   // Relative offset calculations
   uint8_t *out_nonce = tx_buffer;
   uint8_t *out_ciphertext = tx_buffer + NONCE_SIZE;
   uint8_t *out_mac = tx_buffer + NONCE_SIZE + sizeof(DroneData);
  

   // Pre-append the current nonce into the tx_buffer
   memcpy(out_nonce,nonce,NONCE_SIZE);

   // Execute AES-128-GCM encryption and generate the 16-byte MAC using Zero-Copy approach
   int ret = mbedtls_gcm_crypt_and_tag(&gcm_ctx,MBEDTLS_GCM_ENCRYPT,
                                       sizeof(DroneData),
                                       nonce,NONCE_SIZE,
                                       NULL,0, // No Additional Authenticated Data (AAD)
                                      (const unsigned char *)input_data,
                                      out_ciphertext,
                                      MAC_SIZE,
                                      out_mac);
    if (ret != 0) {
        return false;
    }
    
    // Increment the 12-byte nonce (Big-Endian format) for the next cycle
    for(int i = NONCE_SIZE -1 ; i>=0 ; i--){
      nonce[i]++;
      if(nonce[i] != 0){
        break;
      }
    }
     
    return true;
  }


/**
 * @brief Cleans up resources and securely wipes sensitive data from RAM.
 * * @details Operational Flow:
 * -> Free Context : Releases mbedTLS internal memory allocations.
 * -> Zeroization  : Overwrites the nonce and context structures with zeros 
 * to prevent memory extraction.
 */
void security_close(void)
{
  mbedtls_gcm_free(&gcm_ctx);

  //Explicit zeroization
  mbedtls_platform_zeroize(nonce,sizeof(nonce));
  mbedtls_platform_zeroize(&gcm_ctx,sizeof(mbedtls_gcm_context));

}