// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// MBEDTLS module
//
// Purpose: Implements the cryptographic functions used by fp-provider using the
// MBEDTLS package developed by ARM.
//
// To use the package, uncomment the NEARBY_PLATFORM_USE_MBEDTLS define in
// config.mk.
//
// The following features are implemented here:
//
// nearby_platform_Sha256Start(), nearby_platform_Sha256Update(),
// nearby_platform_Sha256Finish()
//
//     Implements the SHA function across and arbitrary length block. Partial
//     blocks can be sent, and the resulting SHA will be over all blocks.
//
// nearby_platform_Aes128Encrypt(), nearby_platform_Aes128Decrypt()
//
//     Encrypt and decrypt a block of data with a given key.
//
// Note that the required function nearby_platform_GenSec256r1Secret() is in a
// separate file, gen_secret.c.
//

#include <mbedtls/aes.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/md.h> /* generic interface */
#include <mbedtls/sha256.h>
#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
#include <mbedtls/compat-2.x.h>
#endif

#include <nearby_platform_se.h>

static mbedtls_sha256_context sha256_ctx;

nearby_platform_status nearby_platform_Sha256Start() {
  nearby_platform_status status = kNearbyStatusError;
  mbedtls_sha256_init(&sha256_ctx);
  if (mbedtls_sha256_starts_ret(&sha256_ctx, 0) == 0) {
    status = kNearbyStatusOK;
  } else {
    mbedtls_sha256_free(&sha256_ctx);
  }
  return status;
}

nearby_platform_status nearby_platform_Sha256Update(const void* data,
                                                    size_t length) {
  nearby_platform_status status = kNearbyStatusError;
  if (mbedtls_sha256_update_ret(&sha256_ctx, (const unsigned char*)data,
                                length) == 0) {
    status = kNearbyStatusOK;
  } else {
    mbedtls_sha256_free(&sha256_ctx);
  }
  return status;
}

nearby_platform_status nearby_platform_Sha256Finish(uint8_t out[32]) {
  nearby_platform_status status = kNearbyStatusError;
  if (mbedtls_sha256_finish_ret(&sha256_ctx, out) == 0) {
    status = kNearbyStatusOK;
  }
  mbedtls_sha256_free(&sha256_ctx);
  return status;
}

/**
 * Encrypts a data block with AES128 in ECB mode.
 */
nearby_platform_status nearby_platform_Aes128Encrypt(
    const uint8_t input[AES_MESSAGE_SIZE_BYTES],
    uint8_t output[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]) {
  nearby_platform_status status = kNearbyStatusError;
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  if (mbedtls_aes_setkey_enc(&ctx, key, 128) != 0) goto exit;
  if (mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, input, output) != 0)
    goto exit;
  status = kNearbyStatusOK;
exit:
  mbedtls_aes_free(&ctx);
  return status;
}

/**
 * Decrypts a data block with AES128 in ECB mode.
 */
nearby_platform_status nearby_platform_Aes128Decrypt(
    const uint8_t input[AES_MESSAGE_SIZE_BYTES],
    uint8_t output[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]) {
  nearby_platform_status status = kNearbyStatusError;
  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);
  if (mbedtls_aes_setkey_dec(&ctx, key, 128) != 0) goto exit;
  if (mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT, input, output) != 0)
    goto exit;
  status = kNearbyStatusOK;
exit:
  mbedtls_aes_free(&ctx);
  return status;
}
