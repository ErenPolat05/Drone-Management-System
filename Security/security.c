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




void security_close(void)
{
  mbedtls_gcm_free(&gcm_ctx);

  //Explicit zeroization
  mbedtls_platform_zeroize(nonce,sizeof(nonce));
  mbedtls_platform_zeroize(&gcm_ctx,sizeof(mbedtls_gcm_context));

}