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
// C API for Rust implementation of LDT [1], tailored to Nearby Presence's
// BLE 4.2 legacy format advertisement parsing usecase.
//
// [1] https://eprint.iacr.org/2017/841.pdf

// TODO pluggable memory allocation for embedded

// TODO include guard name based on final file location
#ifndef NP_LDT_H_
#define NP_LDT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// Individual encrypt/decrypt API, useful when creating advertisements or when
// decrypting advertisements from a known origin

// Handle for accessing the rust ldt implementation apis
typedef uint64_t NpLdtHandle;

// Key material from the Nearby Presence credential from which keys will be
// derived.
typedef struct {
  uint8_t bytes[32];
} NpLdtKeySeed;

typedef struct {
  uint8_t bytes[32];
} NpMetadataKeyHmac;

typedef struct {
  uint8_t bytes[16];
} NpLdtAes128Key;

typedef struct {
  uint8_t bytes[16];
} NpLdtAesBlock;

typedef struct {
  uint8_t bytes[2];
} NpLdtSalt;

// Handle for accessing the AES implementation provided by caller
typedef void* NpLdtAesCipherHandle;

// Create an AES cipher from a key, returning a handle (or 0 on error)
typedef NpLdtAesCipherHandle (*NpLdtAesCreateCipher)(NpLdtAes128Key key);
// Release resources for the cipher, returning 0 on success.
typedef int32_t (*NpLdtAesCloseCipher)(NpLdtAesCipherHandle handle);
// Encrypt the block in place with the cipher corresponding to the handle
typedef void (*NpLdtAesEncrypt)(NpLdtAesCipherHandle handle,
                                NpLdtAesBlock* block);
// Decrypt the block in place with the cipher corresponding to the handle
typedef void (*NpLdtAesDecrypt)(NpLdtAesCipherHandle handle,
                                NpLdtAesBlock* block);

// Functions for creating, destroying, and using AES ciphers.
//
// Users on platforms with optimized AES implementations available may use this
// to have LDT use their preferred AES.
typedef struct {
  NpLdtAesCreateCipher create_cipher;
  NpLdtAesCloseCipher close_cipher;
  NpLdtAesEncrypt encrypt;
  NpLdtAesDecrypt decrypt;
} NpLdtAesConfig;

// Possible result codes returned from the LDT NP API's
typedef enum {
  // Call to api was succesful
  NP_LDT_SUCCESS = 0,
  // Payload of invalid length was provided must be >= 16 and <=31 bytes
  NP_LDT_ERROR_INVALID_LENGTH = -1,
  // The provided metadata hmac did not match the calculated hmac on call to
  // decrypt and verify
  NP_LDT_ERROR_MAC_MISMATCH = -2,
} NP_LDT_RESULT;

// Allocate an LDT-XTS-AES128 cipher using the "swap" mix function.
//
// `aes_config` defines the AES impl that will be used.
// `key_seed` is the key material from the Nearby Presence credential from which
// the LDT key will be derived.
//
// Returns 0 on error, or a non-zero handle on success.
NpLdtHandle NpLdtCreate(NpLdtAesConfig aes_config, NpLdtKeySeed key_seed,
                        NpMetadataKeyHmac known_hmac);

// Release resources for an NpLdtHandle allocated by
// `np_ldt_create_xts_aes_128`.
//
// Returns 0 on success or an NP_LDT_RESULT error code on failure
NP_LDT_RESULT NpLdtClose(NpLdtHandle handle);

// Encrypt a 16-31 byte buffer in-place.
//
// `buffer` is a pointer to a 16-31 byte plaintext, with length in `buffer_len`.
// `salt` is the big-endian  2 byte salt that will be used in the Nearby
// Presence advertisement, which will be incorporated into the tweaks LDT uses
// while encrypting.
//
// Returns 0 on success, in which case `buffer` will now contain ciphertext.
// Returns an NP_LDT_RESULT error code on failure
NP_LDT_RESULT NpLdtEncrypt(NpLdtHandle handle, uint8_t* buffer,
                           size_t buffer_len, NpLdtSalt salt);

// Decrypt a 16-31 byte buffer in-place.
//
// `buffer` is a pointer to a 16-31 byte ciphertext, with length in
// `buffer_len`.
// `salt` is the big-endian  2 byte salt found in the Nearby Presence
// advertisement, which will be incorporated into the tweaks LDT uses while
// decrypting.
//
// Returns 0 on success, in which case `buffer` will now contain plaintext.
// Returns an NP_LDT_RESULT error code on failure
NP_LDT_RESULT NpLdtDecryptAndVerify(NpLdtHandle handle, uint8_t* buffer,
                                    size_t buffer_len, NpLdtSalt salt);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // NP_LDT_H_
