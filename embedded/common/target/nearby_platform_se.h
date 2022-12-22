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

#ifndef NEARBY_PLATFORM_SE_H
#define NEARBY_PLATFORM_SE_H
// clang-format off
#include "nearby_config.h"
// clang-format on
#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Generates a random number.
uint8_t nearby_platform_Rand();

// Computes the sha256 incrementally. Sha256Start() is called first, then
// Sha256Update() one or more times, and finally Sha256Finish().
nearby_platform_status nearby_platform_Sha256Start();

// Intermediate sha256 compute.
//
// data   - Block of data to compute into sha256.
// length - Length of data to process.
nearby_platform_status nearby_platform_Sha256Update(const void* data,
                                                    size_t length);

// Finishes sha256 compute.
//
// out - Contains the final 256 bit sha.
nearby_platform_status nearby_platform_Sha256Finish(uint8_t out[32]);

// Encrypts a data block with AES128 in ECB mode.
//
// input - Input data block to be encrypted.
// output - Resulting encrypted block.
// key    - 128 bit key to use for encryption.
nearby_platform_status nearby_platform_Aes128Encrypt(
    const uint8_t input[AES_MESSAGE_SIZE_BYTES],
    uint8_t output[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]);

// Decrypts a data block with AES128 in ECB mode.
//
// input - Input data block to be decrypted.
// output - Resulting decrypted block.
// key    - 128 bit key to use for decryption.
nearby_platform_status nearby_platform_Aes128Decrypt(
    const uint8_t input[AES_MESSAGE_SIZE_BYTES],
    uint8_t output[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]);

// Generates a shared sec256p1 secret using remote party public key and this
// device's private key.
//
// remote_party_public_key - Remote key.
// secret                  - 256 bit shared secret.
nearby_platform_status nearby_platform_GenSec256r1Secret(
    const uint8_t remote_party_public_key[64], uint8_t secret[32]);

// Returns anti-spoofing 128 bit private key.
// Only used if the implementation also uses the
// nearby_platform_GenSec256r1Secret() routine defined in gen_secret.c.
// Return NULL if not implemented.
const uint8_t* nearby_platform_GetAntiSpoofingPrivateKey();

// Initializes secure element module
nearby_platform_status nearby_platform_SecureElementInit();

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_SE_H */
