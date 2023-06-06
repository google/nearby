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

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <cerrno>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <string>

#include "fakes.h"
#include "nearby_platform_se.h"

#pragma GCC diagnostic ignored "-Wunused-function"

static unsigned int random_value = 0;
static std::queue<uint8_t> random_sequence;

static uint8_t private_key_store[32];

static std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY *)> anti_spoofing_key(
    NULL, EVP_PKEY_free);

static std::string ArrayToString(const uint8_t *data, size_t length) {
  std::stringstream output;
  output << "0x" << std::hex << std::setfill('0') << std::setw(2);
  for (int i = 0; i < length; i++) {
    output << (unsigned)data[i];
  }
  return output.str();
}

// Generates a random number.
uint8_t nearby_platform_Rand() {
  if (random_sequence.empty()) {
    return random_value;
  } else {
    uint8_t v = random_sequence.front();
    random_sequence.pop();
    return v;
  }
}

#ifndef NEARBY_PLATFORM_USE_MBEDTLS
static SHA256_CTX sha256_context;

nearby_platform_status nearby_platform_Sha256Start() {
  SHA256_Init(&sha256_context);
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_Sha256Update(const void *data,
                                                    size_t length) {
  SHA256_Update(&sha256_context, data, length);
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_Sha256Finish(uint8_t out[32]) {
  SHA256_Final(out, &sha256_context);
  return kNearbyStatusOK;
}

// Encrypts a data block with AES128 in ECB mode.
nearby_platform_status nearby_platform_Aes128Encrypt(const uint8_t input[16],
                                                     uint8_t output[16],
                                                     const uint8_t key[16]) {
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int input_length = 16;
  int output_length = 16;

  EVP_EncryptInit(ctx, EVP_aes_128_ecb(), key, NULL);

  if (1 !=
      EVP_EncryptUpdate(ctx, output, &output_length, input, input_length)) {
    return kNearbyStatusError;
  }

  EVP_CIPHER_CTX_free(ctx);
  return kNearbyStatusOK;
}

// Encrypts a data block with AES128 in ECB mode.
nearby_platform_status nearby_platform_Aes128Decrypt(const uint8_t input[16],
                                                     uint8_t output[16],
                                                     const uint8_t key[16]) {
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  int input_length = 16;
  int output_length = 16;

  EVP_DecryptInit(ctx, EVP_aes_128_ecb(), key, NULL);
  EVP_CIPHER_CTX_set_padding(ctx, 0);

  if (1 !=
      EVP_DecryptUpdate(ctx, output, &output_length, input, input_length)) {
    return kNearbyStatusError;
  }

  EVP_CIPHER_CTX_free(ctx);
  return kNearbyStatusOK;
}
#endif /* NEARBY_PLATFORM_USE_MBEDTLS */

static EC_POINT *load_public_key(const uint8_t public_key[64]) {
  BN_CTX *bn_ctx;
  EC_KEY *key;
  EC_POINT *point;
  const EC_GROUP *group;
  uint8_t oct_key[65];

  oct_key[0] = 0x04;
  memcpy(oct_key + 1, public_key, 64);
  bn_ctx = BN_CTX_new();
  key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  group = EC_KEY_get0_group(key);
  point = EC_POINT_new(group);
  if (1 != EC_POINT_oct2point(group, point, oct_key, sizeof(oct_key), bn_ctx))
    return NULL;
  BN_CTX_free(bn_ctx);
  EC_KEY_free(key);
  return point;
}

static BIGNUM *load_private_key(const uint8_t private_key[32]) {
  uint8_t buffer[37];
  buffer[0] = buffer[1] = buffer[2] = 0;
  buffer[3] = 33;
  buffer[4] = 0;
  memcpy(buffer + 5, private_key, 32);
  return BN_mpi2bn(buffer, sizeof(buffer), NULL);
}

#ifdef NEARBY_PLATFORM_HAS_SE
// Generates a shared sec256p1 secret using remote party public key and this
// device's private key.
nearby_platform_status nearby_platform_GenSec256r1Secret(
    const uint8_t remote_party_public_key[64], uint8_t secret[32]) {
  EVP_PKEY_CTX *ctx;
  EVP_PKEY *peerkey;
  EC_POINT *peer_point;
  EC_KEY *ec_peer_key;
  size_t secret_len;

  /* Create the context for the shared secret derivation */
  if (NULL == (ctx = EVP_PKEY_CTX_new(anti_spoofing_key.get(), NULL)))
    return kNearbyStatusError;

  /* Initialise */
  if (1 != EVP_PKEY_derive_init(ctx)) return kNearbyStatusError;

  if (NULL == (peer_point = load_public_key(remote_party_public_key)))
    return kNearbyStatusError;

  ec_peer_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

  if (1 != EC_KEY_set_public_key(ec_peer_key, peer_point))
    return kNearbyStatusError;

  peerkey = EVP_PKEY_new();
  if (1 != EVP_PKEY_assign_EC_KEY(peerkey, ec_peer_key))
    return kNearbyStatusError;

  /* Provide the peer public key */
  if (1 != EVP_PKEY_derive_set_peer(ctx, peerkey)) return kNearbyStatusError;

  /* Determine buffer length for shared secret */
  if (1 != EVP_PKEY_derive(ctx, NULL, &secret_len)) return kNearbyStatusError;

  if (secret_len != 32) return kNearbyStatusError;

  /* Derive the shared secret */
  if (1 != (EVP_PKEY_derive(ctx, secret, &secret_len)))
    return kNearbyStatusError;

  EVP_PKEY_CTX_free(ctx);
  EC_POINT_free(peer_point);
  EVP_PKEY_free(peerkey);

  return kNearbyStatusOK;
}
#endif /* NEARBY_PLATFORM_HAS_SE */

void nearby_test_fakes_SetRandomNumber(unsigned int value) {
  random_value = value;
}

void nearby_test_fakes_SetRandomNumberSequence(std::vector<uint8_t> &value) {
  for (auto &v : value) random_sequence.push(v);
}

nearby_platform_status nearby_test_fakes_SetAntiSpoofingKey(
    const uint8_t private_key[32], const uint8_t public_key[64]) {
  EC_KEY *key;
  BIGNUM *prv;
  EC_POINT *pub;

  prv = load_private_key(private_key);
  if (prv == NULL) return kNearbyStatusError;
  pub = load_public_key(public_key);
  if (pub == NULL) return kNearbyStatusError;

  key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);

  if (1 != EC_KEY_set_private_key(key, prv)) return kNearbyStatusError;
  if (1 != EC_KEY_set_public_key(key, pub)) return kNearbyStatusError;

  anti_spoofing_key.reset(EVP_PKEY_new());
  if (1 != EVP_PKEY_assign_EC_KEY(anti_spoofing_key.get(), key))
    return kNearbyStatusError;

  memcpy(private_key_store, private_key, 32);

  BN_free(prv);
  EC_POINT_free(pub);
  return kNearbyStatusOK;
}

nearby_platform_status nearby_test_fakes_GenSec256r1Secret(
    const uint8_t remote_party_public_key[64], uint8_t secret[32]) {
  return nearby_platform_GenSec256r1Secret(remote_party_public_key, secret);
}

nearby_platform_status nearby_test_fakes_Aes128Decrypt(
    const uint8_t input[AES_MESSAGE_SIZE_BYTES],
    uint8_t output[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]) {
  return nearby_platform_Aes128Decrypt(input, output, key);
}

nearby_platform_status nearby_test_fakes_Aes128Encrypt(
    const uint8_t input[AES_MESSAGE_SIZE_BYTES],
    uint8_t output[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]) {
  return nearby_platform_Aes128Encrypt(input, output, key);
}

const uint8_t *nearby_platform_GetAntiSpoofingPrivateKey() {
  return private_key_store;
}

// Initializes secure element module
nearby_platform_status nearby_platform_SecureElementInit() {
  random_value = 0;
  random_sequence = std::queue<uint8_t>();
  return kNearbyStatusOK;
}
