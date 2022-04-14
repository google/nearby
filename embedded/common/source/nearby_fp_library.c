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

#include "nearby_fp_library.h"

#include <string.h>

#include "nearby.h"
#include "nearby_assert.h"
#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
#include "nearby_platform_battery.h"
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
#include "nearby_platform_bt.h"
#include "nearby_platform_persistence.h"
#include "nearby_platform_se.h"
#include "nearby_trace.h"
#include "nearby_utils.h"

#define ACCOUNT_KEY_LIST_SIZE_BYTES 81
#define SHOW_PAIRING_INDICATION_BYTE 0
#define DONT_SHOW_PAIRING_INDICATION_BYTE 2
#define SHOW_BATTERY_INDICATION_BYTE 0x33
#define DONT_SHOW_BATTERY_INDICATION_BYTE 0x34
#define BATTERY_INFO_CHARGING (1 << 7)
#define BATTERY_INFO_NOT_CHARGING 0
// In the battery values, the highest bit indicates charging, the lower 7 are
// the battery level
#define BATTERY_LEVEL_MASK 0x7F
#define SALT_FIELD_LENGTH_AND_TYPE_BYTE 0x11
#define SALT_SIZE_BYTES 1
#define BATTERY_INFO_SIZE_BYTES 4
#define KEY_BASED_PAIRING_RESPONSE_FLAG 0x01
#define GAP_DATA_TYPE_SERVICE_DATA_UUID 0x16
#define FP_SERVICE_UUID 0xFE2C
#define GAP_DATA_TYPE_TX_POWER_LEVEL_UUID 0x0A
#define TX_POWER_DATA_SIZE 2

static uint8_t account_key_list[ACCOUNT_KEY_LIST_SIZE_BYTES];

static uint8_t sha_buffer[32];
static uint8_t key_and_salt[ACCOUNT_KEY_SIZE_BYTES + SALT_SIZE_BYTES +
                            BATTERY_INFO_SIZE_BYTES];

static size_t GetAccountKeyListUsedSize() {
  return nearby_fp_GetAccountKeyOffset(nearby_fp_GetAccountKeyCount());
}

size_t nearby_fp_GetAccountKeyCount() { return account_key_list[0]; }

size_t nearby_fp_GetAccountKeyOffset(unsigned key_number) {
  return 1 + key_number * ACCOUNT_KEY_SIZE_BYTES;
}

const uint8_t* nearby_fp_GetAccountKey(unsigned key_number) {
  NEARBY_ASSERT(key_number < nearby_fp_GetAccountKeyCount());
  return account_key_list + nearby_fp_GetAccountKeyOffset(key_number);
}

void nearby_fp_MarkAccountKeyAsActive(unsigned key_number) {
  NEARBY_ASSERT(key_number < nearby_fp_GetAccountKeyCount());
  uint8_t tmp[ACCOUNT_KEY_SIZE_BYTES];
  if (key_number == 0) return;
  // Move the key to the top of the list
  nearby_fp_CopyAccountKey(tmp, key_number);
  memmove(account_key_list + nearby_fp_GetAccountKeyOffset(1),
          account_key_list + nearby_fp_GetAccountKeyOffset(0),
          key_number * ACCOUNT_KEY_SIZE_BYTES);
  memcpy(account_key_list + nearby_fp_GetAccountKeyOffset(0), tmp,
         ACCOUNT_KEY_SIZE_BYTES);
}

void nearby_fp_CopyAccountKey(uint8_t* dest, unsigned key_number) {
  size_t offset = nearby_fp_GetAccountKeyOffset(key_number);
  NEARBY_ASSERT(offset + ACCOUNT_KEY_SIZE_BYTES <= ACCOUNT_KEY_LIST_SIZE_BYTES);
  memcpy(dest, account_key_list + offset, ACCOUNT_KEY_SIZE_BYTES);
}

static unsigned combineNibbles(unsigned high, unsigned low) {
  return ((high << 4) & 0xF0) | (low & 0x0F);
}

void nearby_fp_AddAccountKey(const uint8_t key[ACCOUNT_KEY_SIZE_BYTES]) {
  // Find if the key is already on the list
  unsigned i;
  size_t key_count;
  size_t length;
  size_t max_bytes;
  key_count = nearby_fp_GetAccountKeyCount();
  for (i = 0; i < key_count; i++) {
    if (!memcmp(key, nearby_fp_GetAccountKey(i), ACCOUNT_KEY_SIZE_BYTES)) {
      nearby_fp_MarkAccountKeyAsActive(i);
      return;
    }
  }
  // Insert `key` at the list top
  length = key_count * ACCOUNT_KEY_SIZE_BYTES;
  max_bytes = ACCOUNT_KEY_LIST_SIZE_BYTES - nearby_fp_GetAccountKeyOffset(1);
  if (length > max_bytes) {
    // Buffer is full, the last key will fall off the edge
    length = max_bytes;
  } else {
    // We have room for one more key
    account_key_list[0]++;
  }
  memmove(account_key_list + nearby_fp_GetAccountKeyOffset(1),
          account_key_list + nearby_fp_GetAccountKeyOffset(0), length);
  memcpy(account_key_list + nearby_fp_GetAccountKeyOffset(0), key,
         ACCOUNT_KEY_SIZE_BYTES);
}

size_t nearby_fp_CreateDiscoverableAdvertisement(uint8_t* output,
                                                 size_t length) {
  NEARBY_ASSERT(length >= DISCOVERABLE_ADV_SIZE_BYTES);

  size_t i = 0;

  // data size
  output[i++] = 1 + FP_SERVICE_UUID_SIZE + FP_MODEL_ID_SIZE;

  // data type service
  output[i++] = GAP_DATA_TYPE_SERVICE_DATA_UUID;

  // service data uuid
  nearby_utils_CopyLittleEndian(output + i, FP_SERVICE_UUID,
                                FP_SERVICE_UUID_SIZE);
  i += FP_SERVICE_UUID_SIZE;

  // service data
  uint32_t model = nearby_platform_GetModelId();
  nearby_utils_CopyBigEndian(output + i, model, FP_MODEL_ID_SIZE);
  i += FP_MODEL_ID_SIZE;

  return i;
}

#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
void SerializeBatteryInfo(uint8_t* output,
                          const nearby_platform_BatteryInfo* battery_info) {
  uint8_t charging = battery_info->is_charging ? BATTERY_INFO_CHARGING
                                               : BATTERY_INFO_NOT_CHARGING;
  output[0] =
      charging | (battery_info->left_bud_battery_level & BATTERY_LEVEL_MASK);
  output[1] =
      charging | (battery_info->right_bud_battery_level & BATTERY_LEVEL_MASK);
  output[2] = charging |
              (battery_info->charging_case_battery_level & BATTERY_LEVEL_MASK);
}
static void AddBatteryInfo(uint8_t* output, size_t length,
                           bool show_ui_indicator,
                           const nearby_platform_BatteryInfo* battery_info) {
  NEARBY_ASSERT(length >= BATTERY_INFO_SIZE_BYTES);
  output[0] = show_ui_indicator ? SHOW_BATTERY_INDICATION_BYTE
                                : DONT_SHOW_BATTERY_INDICATION_BYTE;
  SerializeBatteryInfo(output + 1, battery_info);
}

#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

static size_t CreateNondiscoverableAdvertisement(
    uint8_t* output, size_t length, bool show_pairing_indicator
#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
    ,
    bool show_battery_indicator, const nearby_platform_BatteryInfo* battery_info
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
) {
  NEARBY_TRACE(VERBOSE, "nearby_fp_CreateNondiscoverableAdvertisement");
  const unsigned kHeaderSize = FP_SERVICE_UUID_SIZE + 2;
  NEARBY_ASSERT(length >= kHeaderSize);

  unsigned i = 1;

  // service data UUID
  output[i++] = GAP_DATA_TYPE_SERVICE_DATA_UUID;

  // service uuid
  nearby_utils_CopyLittleEndian(output + i, FP_SERVICE_UUID,
                                FP_SERVICE_UUID_SIZE);
  i += FP_SERVICE_UUID_SIZE;

  // service data
  uint8_t salt = nearby_platform_Rand();
  size_t n = nearby_fp_GetAccountKeyCount();
  if (n == 0) {
    const unsigned kMessageSize = 2;
    NEARBY_ASSERT(length >= i + kMessageSize);
    output[i++] = 0x00;
    output[i++] = 0x00;
  } else {
    const unsigned kFlagFilterAndSaltSize = 4;
    const size_t s = (6 * n + 15) / 5;
    const size_t kAccountKeyDataSize = s + kFlagFilterAndSaltSize;
    NEARBY_ASSERT(length >= i + kAccountKeyDataSize);
    unsigned used_key_and_salt_size = ACCOUNT_KEY_SIZE_BYTES + SALT_SIZE_BYTES;
#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
    // We need to add the battery info first because it's used as salt
    if (battery_info != NULL) {
      uint8_t battery_chunk_offset = i + kAccountKeyDataSize;
      uint8_t* battery_chunk = output + battery_chunk_offset;
      used_key_and_salt_size += BATTERY_INFO_SIZE_BYTES;
      AddBatteryInfo(battery_chunk, length - battery_chunk_offset,
                     show_battery_indicator, battery_info);
      memcpy(key_and_salt + ACCOUNT_KEY_SIZE_BYTES + SALT_SIZE_BYTES,
             battery_chunk, BATTERY_INFO_SIZE_BYTES);
    }
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
    // flags
    output[i++] = 0;
    // filter length and type
    output[i++] = combineNibbles(s, show_pairing_indicator
                                        ? SHOW_PAIRING_INDICATION_BYTE
                                        : DONT_SHOW_PAIRING_INDICATION_BYTE);
    memset(output + i, 0, s);
    key_and_salt[ACCOUNT_KEY_SIZE_BYTES] = salt;
    unsigned k, j;
    for (k = 0; k < n; k++) {
      nearby_fp_CopyAccountKey(key_and_salt, k);
      nearby_fp_Sha256(sha_buffer, key_and_salt, used_key_and_salt_size);
      for (j = 0; j < 8; j++) {
        uint32_t x = nearby_utils_GetBigEndian32(sha_buffer + 4 * j);
        uint32_t m = x % (s * 8);
        output[i + (m / 8)] |= (1 << (m % 8));
      }
    }
    i += s;
    output[i++] = SALT_FIELD_LENGTH_AND_TYPE_BYTE;
    output[i++] = salt;
#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
    if (battery_info != NULL) {
      i += BATTERY_INFO_SIZE_BYTES;
    }
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
  }

  // service data size
  output[0] = i - 1;

  return i;
}

size_t nearby_fp_CreateNondiscoverableAdvertisement(
    uint8_t* output, size_t length, bool show_pairing_indicator) {
#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
  return CreateNondiscoverableAdvertisement(
      output, length, show_pairing_indicator, false, NULL);
#else
  return CreateNondiscoverableAdvertisement(output, length,
                                            show_pairing_indicator);
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
}

#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
// |battery_info| can be NULL
size_t nearby_fp_CreateNondiscoverableAdvertisementWithBattery(
    uint8_t* output, size_t length, bool show_pairing_indicator,
    bool show_battery_indicator,
    const nearby_platform_BatteryInfo* battery_info) {
  return CreateNondiscoverableAdvertisement(
      output, length, show_pairing_indicator, show_battery_indicator,
      battery_info);
}
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

size_t nearby_fp_AppendTxPower(uint8_t* advertisement, size_t length,
                               int8_t tx_power) {
  size_t offset = 0;

  NEARBY_ASSERT(length >= 1 + TX_POWER_DATA_SIZE);

  // tx power level data size
  advertisement[offset++] = TX_POWER_DATA_SIZE;

  // tx power level UUID
  advertisement[offset++] = GAP_DATA_TYPE_TX_POWER_LEVEL_UUID;

  // tx power level
  advertisement[offset++] = tx_power;

  return offset;
}

nearby_platform_status nearby_fp_LoadAccountKeys() {
  size_t length = sizeof(account_key_list);
  memset(account_key_list, 0, length);
  return nearby_platform_LoadValue(kStoredKeyAccountKeyList, account_key_list,
                                   &length);
}

nearby_platform_status nearby_fp_SaveAccountKeys() {
  return nearby_platform_SaveValue(kStoredKeyAccountKeyList, account_key_list,
                                   GetAccountKeyListUsedSize());
}

nearby_platform_status nearby_fp_GattReadModelId(uint8_t* output,
                                                 size_t* length) {
  NEARBY_ASSERT(*length >= FP_MODEL_ID_SIZE);
  uint32_t model = nearby_platform_GetModelId();
  nearby_utils_CopyBigEndian(output, model, FP_MODEL_ID_SIZE);
  *length = FP_MODEL_ID_SIZE;
  return kNearbyStatusOK;
}

nearby_platform_status nearby_fp_CreateSharedSecret(
    const uint8_t remote_public_key[64],
    uint8_t output[ACCOUNT_KEY_SIZE_BYTES]) {
  nearby_platform_status status;
  uint8_t secret[32];
  uint8_t hash[32];
  status = nearby_platform_GenSec256r1Secret(remote_public_key, secret);
  if (status != kNearbyStatusOK) return status;

  status = nearby_fp_Sha256(hash, secret, sizeof(secret));
  if (status != kNearbyStatusOK) return status;

  memcpy(output, hash, ACCOUNT_KEY_SIZE_BYTES);
  return status;
}

nearby_platform_status nearby_fp_CreateRawKeybasedPairingResponse(
    uint8_t output[AES_MESSAGE_SIZE_BYTES]) {
  output[0] = KEY_BASED_PAIRING_RESPONSE_FLAG;
  nearby_utils_CopyBigEndian(output + 1, nearby_platform_GetPublicAddress(),
                             BT_ADDRESS_LENGTH);
  int i;
  for (i = 1 + BT_ADDRESS_LENGTH; i < AES_MESSAGE_SIZE_BYTES; i++) {
    output[i] = nearby_platform_Rand();
  }
  return kNearbyStatusOK;
}

#ifdef NEARBY_FP_ENABLE_ADDITIONAL_DATA
#define HMAC_SHA256_KEY_SIZE 64
#define OPAD 0x5C
#define IPAD 0x36
#define NONCE_SIZE 8
#define ADDITIONAL_DATA_SHA_SIZE 8

static void PadKey(uint8_t output[HMAC_SHA256_KEY_SIZE], const uint8_t* key,
                   size_t key_length, uint8_t pad) {
  int i;
  for (i = 0; i < key_length; i++) {
    *output++ = *key++ ^ pad;
  }
  memset(output, pad, HMAC_SHA256_KEY_SIZE - key_length);
}

#define RETURN_IF_ERROR(X)                        \
  do {                                            \
    nearby_platform_status status = X;            \
    if (kNearbyStatusOK != status) return status; \
  } while (0)

static nearby_platform_status HmacSha256(uint8_t out[32],
                                         uint8_t hmac_key[HMAC_SHA256_KEY_SIZE],
                                         const uint8_t* data,
                                         size_t data_length) {
  RETURN_IF_ERROR(nearby_platform_Sha256Start());
  RETURN_IF_ERROR(nearby_platform_Sha256Update(hmac_key, HMAC_SHA256_KEY_SIZE));
  RETURN_IF_ERROR(nearby_platform_Sha256Update(data, data_length));
  RETURN_IF_ERROR(nearby_platform_Sha256Finish(out));
  return kNearbyStatusOK;
}

nearby_platform_status nearby_fp_HmacSha256(uint8_t out[32], const uint8_t* key,
                                            size_t key_length,
                                            const uint8_t* data,
                                            size_t data_length) {
  uint8_t hmac_key[HMAC_SHA256_KEY_SIZE];
  // out = HASH(Key XOR ipad, data)
  PadKey(hmac_key, key, key_length, IPAD);
  RETURN_IF_ERROR(HmacSha256(out, hmac_key, data, data_length));
  // out = HASH(Key XOR opad, out)
  PadKey(hmac_key, key, key_length, OPAD);
  return HmacSha256(out, hmac_key, out, 32);
}

nearby_platform_status nearby_fp_AesCtr(
    uint8_t* message, size_t message_length,
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]) {
  uint8_t key_stream[AES_MESSAGE_SIZE_BYTES];
  uint8_t iv[AES_MESSAGE_SIZE_BYTES];
  size_t offset = NONCE_SIZE;
  memset(iv, 0, AES_MESSAGE_SIZE_BYTES - NONCE_SIZE);
  memcpy(iv + AES_MESSAGE_SIZE_BYTES - NONCE_SIZE, message, NONCE_SIZE);

  while (offset < message_length) {
    int i;
    int bytes_left = message_length - offset;
    RETURN_IF_ERROR(nearby_platform_Aes128Encrypt(iv, key_stream, key));
    for (i = 0; i < bytes_left && i < sizeof(key_stream); i++) {
      message[offset++] ^= key_stream[i];
    }
    iv[0]++;
  }
  return kNearbyStatusOK;
}

nearby_platform_status nearby_fp_DecodeAdditionalData(
    uint8_t* data, size_t length, const uint8_t key[ACCOUNT_KEY_SIZE_BYTES]) {
  NEARBY_ASSERT(length > ADDITIONAL_DATA_SHA_SIZE + NONCE_SIZE);
  uint8_t sha[32];
  RETURN_IF_ERROR(nearby_fp_HmacSha256(sha, key, ACCOUNT_KEY_SIZE_BYTES,
                                       data + ADDITIONAL_DATA_SHA_SIZE,
                                       length - ADDITIONAL_DATA_SHA_SIZE));
  if (memcmp(sha, data, ADDITIONAL_DATA_SHA_SIZE) != 0) {
    NEARBY_TRACE(WARNING, "Additional Data SHA check failed");
    return kNearbyStatusError;
  }
  return nearby_fp_AesCtr(data + ADDITIONAL_DATA_SHA_SIZE,
                          length - ADDITIONAL_DATA_SHA_SIZE, key);
}

nearby_platform_status nearby_fp_EncodeAdditionalData(
    uint8_t* data, size_t length, const uint8_t key[ACCOUNT_KEY_SIZE_BYTES]) {
  NEARBY_ASSERT(length >= ADDITIONAL_DATA_SHA_SIZE + NONCE_SIZE);
  uint8_t sha[32];
  int i;
  for (i = 0; i < NONCE_SIZE; i++) {
    data[ADDITIONAL_DATA_SHA_SIZE + i] = nearby_platform_Rand();
  }
  RETURN_IF_ERROR(nearby_fp_AesCtr(data + ADDITIONAL_DATA_SHA_SIZE,
                                   length - ADDITIONAL_DATA_SHA_SIZE, key));
  RETURN_IF_ERROR(nearby_fp_HmacSha256(sha, key, ACCOUNT_KEY_SIZE_BYTES,
                                       data + ADDITIONAL_DATA_SHA_SIZE,
                                       length - ADDITIONAL_DATA_SHA_SIZE));
  memcpy(data, sha, ADDITIONAL_DATA_SHA_SIZE);
  return kNearbyStatusOK;
}
#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */

// Computes sha256 sum.
nearby_platform_status nearby_fp_Sha256(uint8_t out[32], const void* data,
                                        size_t length) {
  nearby_platform_status status = nearby_platform_Sha256Start();
  if (status == kNearbyStatusOK) {
    status = nearby_platform_Sha256Update(data, length);
    if (status == kNearbyStatusOK) {
      status = nearby_platform_Sha256Finish(out);
    }
  }
  return status;
}
