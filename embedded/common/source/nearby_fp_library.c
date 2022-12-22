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
#include "nearby_message_stream.h"
#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
#include "nearby_platform_battery.h"
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

#include "nearby_platform_audio.h"
#include "nearby_platform_bt.h"
#include "nearby_platform_persistence.h"
#include "nearby_platform_se.h"
#include "nearby_trace.h"
#include "nearby_utils.h"

typedef struct {
  uint8_t num_keys;
  nearby_platform_AccountKeyInfo key[NEARBY_MAX_ACCOUNT_KEYS];
} AccountKeyList;

// Advertisement header with SASS format (version 1)
#define SASS_HEADER 0x10

// Offset to the advertisement header
#define HEADER_OFFSET 4

// Offset to account data in a non-discoverable advertisement
#define ACCOUNT_KEY_DATA_OFFSET 5

#define LTV_HEADER_SIZE 1
#define ACCOUNT_KEY_LIST_SIZE_BYTES sizeof(AccountKeyList)
#define SHOW_PAIRING_INDICATION_BYTE 0
#define DONT_SHOW_PAIRING_INDICATION_BYTE 2
#define SHOW_BATTERY_INDICATION_BYTE 0x33
#define DONT_SHOW_BATTERY_INDICATION_BYTE 0x34
#define BATTERY_INFO_CHARGING (1 << 7)
#define BATTERY_INFO_NOT_CHARGING 0
// In the battery values, the highest bit indicates charging, the lower 7 are
// the battery level
#define BATTERY_LEVEL_MASK 0x7F
#define SALT_FIELD_LENGTH_AND_TYPE_BYTE ((NEARBY_FP_SALT_SIZE << 4) | 1)
#define BATTERY_INFO_SIZE_BYTES 4
// Response for seekers that don't support BLE-only devices.
#define KEY_BASED_PAIRING_RESPONSE_FLAG 0x01
// Response for seekers that support BLE-only devices.
#define KEY_BASED_PAIRING_EXTENDED_RESPONSE_FLAG 0x02
#define GAP_DATA_TYPE_SERVICE_DATA_UUID 0x16
#define FP_SERVICE_UUID 0xFE2C
#define GAP_DATA_TYPE_TX_POWER_LEVEL_UUID 0x0A
#define TX_POWER_DATA_SIZE 2
#define SASS_HEADER_SIZE 1

// (type + length) + state + custom_data
#define MIN_SASS_ADERTISEMENT_SIZE 3

#define SASS_CONN_STATE_ON_HEAD_OFFSET 7
#define SASS_CONN_STATE_AVAIL_OFFSET 6
#define SASS_CONN_STATE_FOCUS_OFFSET 5
#define SASS_CONN_STATE_AUTO_RECONNECTED_OFFSET 4
#define SASS_CONN_STATE_MASK 0x0F

#define MOST_RECENTLY_USED_ACCOUNT_KEY_BIT 0x01
#define IN_USE_ACCOUNT_KEY_BIT 0x02

// SASS Configuration Flags
#define SASS_CF_ON_OFFSET 15
#define SASS_CF_MULTIPOINT_CONFIGURABLE 14
#define SASS_CF_MULTIPOINT_ON 13
#define SASS_CF_OHD_SUPPORTED 12
#define SASS_CF_OHD_ENABLED 11

#define MESSAGE_AUTHENTICATION_CODE_SIZE 8

#define BOOL_TO_INT(x) ((x) ? 1 : 0)

static const uint8_t kSassRrdKey[] = {'S', 'A', 'S', 'S', '-', 'R',
                                      'R', 'D', '-', 'K', 'E', 'Y'};

static AccountKeyList account_key_list;

static uint8_t sha_buffer[32];

#define RETURN_IF_ERROR(X)                        \
  do {                                            \
    nearby_platform_status status = X;            \
    if (kNearbyStatusOK != status) return status; \
  } while (0)

// Returns the Length part of Length|Type field.
static int GetLtLength(uint8_t value) { return value >> 4; }

// Returns the Type part of Length|Type field.
static int GetLtType(uint8_t value) { return value & 0x0F; }

const uint8_t* nearby_fp_FindLtv(const uint8_t* advertisement, int type) {
  size_t offset = ACCOUNT_KEY_DATA_OFFSET;
  size_t advertisement_length = advertisement[0];
  while (offset < advertisement_length) {
    if (type == GetLtType(advertisement[offset])) {
      return advertisement + offset;
    }
    offset += GetLtLength(advertisement[offset]) + LTV_HEADER_SIZE;
  }
  return NULL;
}

static size_t GetAccountKeyListUsedSize() { return sizeof(account_key_list); }

// Returns true if |account_key| is present in [0..end_offset) range in acount
// key list
static bool IsAccountKeyInRange(const uint8_t* account_key, size_t end_offset) {
  for (size_t i = 0; i < end_offset; i++) {
    if (!memcmp(account_key, nearby_fp_GetAccountKey(i)->account_key,
                ACCOUNT_KEY_SIZE_BYTES)) {
      return true;
    }
  }
  return false;
}

size_t nearby_fp_GetAccountKeyCount() { return account_key_list.num_keys; }

size_t nearby_fp_GetUniqueAccountKeyCount() {
  size_t count = 0;
  int offset = 0;
  while (true) {
    offset = nearby_fp_GetNextUniqueAccountKeyIndex(offset);
    if (offset == -1) break;
    count++;
    offset++;
  }
  return count;
}

int nearby_fp_GetNextUniqueAccountKeyIndex(int offset) {
  while (offset < nearby_fp_GetAccountKeyCount()) {
    const nearby_platform_AccountKeyInfo* account_key_info =
        nearby_fp_GetAccountKey(offset);
    if (IsAccountKeyInRange(account_key_info->account_key, offset)) {
      offset++;
    } else {
      return offset;
    }
  }
  return -1;
}

const nearby_platform_AccountKeyInfo* nearby_fp_GetAccountKey(
    unsigned key_number) {
  NEARBY_ASSERT(key_number < nearby_fp_GetAccountKeyCount());
  return &account_key_list.key[key_number];
}

void nearby_fp_MarkAccountKeyAsActive(unsigned key_number) {
  NEARBY_ASSERT(key_number < nearby_fp_GetAccountKeyCount());
  if (key_number == 0) return;
  // Move the key to the top of the list
  nearby_platform_AccountKeyInfo tmp = account_key_list.key[key_number];
  for (unsigned i = key_number; i > 0; i--) {
    account_key_list.key[i] = account_key_list.key[i - 1];
  }
  account_key_list.key[0] = tmp;
}

void nearby_fp_CopyAccountKey(nearby_platform_AccountKeyInfo* dest,
                              unsigned key_number) {
  NEARBY_ASSERT(key_number < nearby_fp_GetAccountKeyCount());
  *dest = account_key_list.key[key_number];
}

static unsigned combineNibbles(unsigned high, unsigned low) {
  return ((high << 4) & 0xF0) | (low & 0x0F);
}

// Returns |a| == |b|
static bool AccountKeyInfoEquals(const nearby_platform_AccountKeyInfo* a,
                                 const nearby_platform_AccountKeyInfo* b) {
  return a == b ||
         (
#ifdef NEARBY_FP_ENABLE_SASS
             a->peer_address == b->peer_address &&
#endif /* NEARBY_FP_ENABLE_SASS */
             !memcmp(a->account_key, b->account_key, ACCOUNT_KEY_SIZE_BYTES));
}

void nearby_fp_AddAccountKey(const nearby_platform_AccountKeyInfo* key) {
  // Find if the key is already on the list
  unsigned i;
  size_t key_count;
  key_count = nearby_fp_GetAccountKeyCount();
  for (i = 0; i < key_count; i++) {
    if (AccountKeyInfoEquals(key, nearby_fp_GetAccountKey(i))) {
      nearby_fp_MarkAccountKeyAsActive(i);
      return;
    }
  }
  // Insert `key` at the list top. If the list is full, the last key will fall
  // off the edge
  size_t keys_to_copy = key_count < NEARBY_MAX_ACCOUNT_KEYS
                            ? key_count
                            : NEARBY_MAX_ACCOUNT_KEYS - 1;
  for (i = keys_to_copy; i > 0; i--) {
    account_key_list.key[i] = account_key_list.key[i - 1];
  }
  account_key_list.key[0] = *key;
  if (key_count < NEARBY_MAX_ACCOUNT_KEYS) {
    account_key_list.num_keys++;
  }
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

#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
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
#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
    ,
    bool show_battery_indicator, const nearby_platform_BatteryInfo* battery_info
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
) {
  NEARBY_TRACE(VERBOSE, "nearby_fp_CreateNondiscoverableAdvertisement");
  const unsigned kHeaderSize = FP_SERVICE_UUID_SIZE + 2;
  NEARBY_ASSERT(length >= kHeaderSize);

  unsigned i = 1;
  uint8_t salt[NEARBY_FP_SALT_SIZE];

  // service data UUID
  output[i++] = GAP_DATA_TYPE_SERVICE_DATA_UUID;

  // service uuid
  nearby_utils_CopyLittleEndian(output + i, FP_SERVICE_UUID,
                                FP_SERVICE_UUID_SIZE);
  i += FP_SERVICE_UUID_SIZE;

  // service data
  for (int si = 0; si < NEARBY_FP_SALT_SIZE; si++)
    salt[si] = nearby_platform_Rand();
  size_t n = nearby_fp_GetUniqueAccountKeyCount();
  if (n == 0) {
    const unsigned kMessageSize = 2;
    NEARBY_ASSERT(length >= i + kMessageSize);
    output[i++] = 0x00;
    output[i++] = 0x00;
  } else {
    const unsigned kFlagFilterAndSaltSize = 3 + NEARBY_FP_SALT_SIZE;
    const size_t s = (6 * n + 15) / 5;
    const size_t kAccountKeyDataSize = s + kFlagFilterAndSaltSize;
    NEARBY_ASSERT(length >= i + kAccountKeyDataSize);
#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
    // We need to add the battery info first because it's used as salt
    if (battery_info != NULL) {
      uint8_t battery_chunk_offset = i + kAccountKeyDataSize;
      uint8_t* battery_chunk = output + battery_chunk_offset;
      AddBatteryInfo(battery_chunk, length - battery_chunk_offset,
                     show_battery_indicator, battery_info);
    }
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
    // flags
    output[i++] = 0;
    // filter length and type
    output[i++] = combineNibbles(s, show_pairing_indicator
                                        ? SHOW_PAIRING_INDICATION_BYTE
                                        : DONT_SHOW_PAIRING_INDICATION_BYTE);
    memset(output + i, 0, s);
    i += s;
    output[i++] = SALT_FIELD_LENGTH_AND_TYPE_BYTE;
    for (int si = 0; si < NEARBY_FP_SALT_SIZE; si++) output[i++] = salt[si];
#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
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
#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
  return CreateNondiscoverableAdvertisement(
      output, length, show_pairing_indicator, false, NULL);
#else
  return CreateNondiscoverableAdvertisement(output, length,
                                            show_pairing_indicator);
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
}

#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
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

static const uint8_t* FindBatteryInfoLt(const uint8_t* advertisement) {
  const uint8_t* battery_info = nearby_fp_FindLtv(
      advertisement, BATTERY_INFO_WITH_UI_INDICATION_FIELD_TYPE);
  if (battery_info == NULL) {
    battery_info = nearby_fp_FindLtv(
        advertisement, BATTERY_INFO_WITHOUT_UI_INDICATION_FIELD_TYPE);
  }
  return battery_info;
}

size_t nearby_fp_SetBloomFilter(uint8_t* advertisement, bool use_sass_format,
                                const uint8_t* in_use_key) {
  unsigned key_offset = 0;
  if (advertisement[ACCOUNT_KEY_DATA_OFFSET] == 0) {
    NEARBY_TRACE(INFO, "Empty account key filter");
    return 0;
  }
  // Salt is mandatory and is included in the calculation without the LT header
  const uint8_t* salt_field = nearby_fp_FindLtv(advertisement, SALT_FIELD_TYPE);
  NEARBY_ASSERT(salt_field != NULL);
  const uint8_t* salt = salt_field + LTV_HEADER_SIZE;
  int salt_length = GetLtLength(*salt_field);
  // Battery info is optional and is included in the calculation with the LT
  // header
  const uint8_t* battery_info_field = FindBatteryInfoLt(advertisement);
  int battery_info_field_length =
      battery_info_field != NULL
          ? GetLtLength(*battery_info_field) + LTV_HEADER_SIZE
          : 0;
  // Random resolvable field is optional and is included in the calculation with
  // the LT header
  const uint8_t* random_resolvable_field =
      nearby_fp_FindLtv(advertisement, RANDOM_RESOLVABLE_FIELD_TYPE);
  int random_resolvable_field_length =
      random_resolvable_field != NULL
          ? GetLtLength(*random_resolvable_field) + LTV_HEADER_SIZE
          : 0;
  const size_t n = nearby_fp_GetUniqueAccountKeyCount();
  const size_t s = (6 * n + 15) / 5;
  NEARBY_ASSERT(s == GetLtLength(advertisement[ACCOUNT_KEY_DATA_OFFSET]));
  uint8_t* output = advertisement + ACCOUNT_KEY_DATA_OFFSET + LTV_HEADER_SIZE;
  memset(output, 0, s);
  for (size_t k = 0; k < n; k++) {
    key_offset = nearby_fp_GetNextUniqueAccountKeyIndex(key_offset);
    NEARBY_ASSERT(key_offset >= 0);
    const uint8_t* key = nearby_fp_GetAccountKey(key_offset)->account_key;
    uint8_t flags = key[0];
    if (use_sass_format) {
      if (in_use_key != NULL) {
        if (!memcmp(key, in_use_key, ACCOUNT_KEY_SIZE_BYTES)) {
          flags |= IN_USE_ACCOUNT_KEY_BIT;
        }
      } else if (k == 0) {
        // The first key is the most recently used one
        flags |= MOST_RECENTLY_USED_ACCOUNT_KEY_BIT;
      }
    }
    key_offset++;
    nearby_platform_Sha256Start();
    nearby_platform_Sha256Update(&flags, sizeof(flags));
    nearby_platform_Sha256Update(key + sizeof(flags),
                                 ACCOUNT_KEY_SIZE_BYTES - sizeof(flags));
    nearby_platform_Sha256Update(salt, salt_length);
    nearby_platform_Sha256Update(battery_info_field, battery_info_field_length);
    nearby_platform_Sha256Update(random_resolvable_field,
                                 random_resolvable_field_length);
    nearby_platform_Sha256Finish(sha_buffer);
    for (unsigned j = 0; j < 8; j++) {
      uint32_t x = nearby_utils_GetBigEndian32(sha_buffer + 4 * j);
      uint32_t m = x % (s * 8);
      output[m / 8] |= (1 << (m % 8));
    }
  }
  if (use_sass_format) {
    advertisement[HEADER_OFFSET] = SASS_HEADER;
  }
  return s;
}

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
  memset(&account_key_list, 0, length);
  return nearby_platform_LoadValue(kStoredKeyAccountKeyList,
                                   (uint8_t*)&account_key_list, &length);
}

nearby_platform_status nearby_fp_SaveAccountKeys() {
  return nearby_platform_SaveValue(kStoredKeyAccountKeyList,
                                   (uint8_t*)&account_key_list,
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
    uint8_t output[AES_MESSAGE_SIZE_BYTES], bool extended_response) {
  int i = 0;
  uint64_t secondary_address = 0;
  if (extended_response) {
    secondary_address = nearby_platform_GetSecondaryPublicAddress();
    output[i++] = KEY_BASED_PAIRING_EXTENDED_RESPONSE_FLAG;
    uint8_t flags = 0;
#if NEARBY_FP_BLE_ONLY
    flags |= 0x80;
#endif /* NEARBY_FP_BLE_ONLY */
#if NEARBY_FP_PREFER_BLE_BONDING
    flags |= 0x40;
#endif /* NEARBY_FP_PREFER_BLE_BONDING */
#if NEARBY_FP_PREFER_LE_TRANSPORT
    flags |= 0x20;
#endif /* NEARBY_FP_PREFER_LE_TRANSPORT */
    output[i++] = flags;
    // Number of the Provider’s addresses. The number is either 1 or 2.
    output[i++] = secondary_address != 0 ? 2 : 1;
  } else {
    output[i++] = KEY_BASED_PAIRING_RESPONSE_FLAG;
  }
  nearby_utils_CopyBigEndian(&output[i], nearby_platform_GetPublicAddress(),
                             BT_ADDRESS_LENGTH);
  i += BT_ADDRESS_LENGTH;
  if (secondary_address != 0) {
    nearby_utils_CopyBigEndian(&output[i], secondary_address,
                               BT_ADDRESS_LENGTH);
    i += BT_ADDRESS_LENGTH;
  }
  for (; i < AES_MESSAGE_SIZE_BYTES; i++) {
    output[i] = nearby_platform_Rand();
  }
  return kNearbyStatusOK;
}

#define OPAD 0x5C
#define IPAD 0x36
#define NONCE_SIZE 8

static void PadKey(uint8_t output[HMAC_SHA256_KEY_SIZE], const uint8_t* key,
                   size_t key_length, uint8_t pad) {
  size_t i;
  for (i = 0; i < key_length; i++) {
    *output++ = *key++ ^ pad;
  }
  memset(output, pad, HMAC_SHA256_KEY_SIZE - key_length);
}

static nearby_platform_status HmacSha256(uint8_t out[SHA256_KEY_SIZE],
                                         uint8_t hmac_key[HMAC_SHA256_KEY_SIZE],
                                         const uint8_t* data,
                                         size_t data_length) {
  RETURN_IF_ERROR(nearby_platform_Sha256Start());
  RETURN_IF_ERROR(nearby_platform_Sha256Update(hmac_key, HMAC_SHA256_KEY_SIZE));
  RETURN_IF_ERROR(nearby_platform_Sha256Update(data, data_length));
  return nearby_platform_Sha256Finish(out);
}

static nearby_platform_status HmacSha256WithNonce(
    uint8_t out[32], uint8_t hmac_key[HMAC_SHA256_KEY_SIZE],
    const uint8_t session_nonce[SESSION_NONCE_SIZE],
    const uint8_t message_nonce[SESSION_NONCE_SIZE], const uint8_t* data,
    size_t data_length) {
  RETURN_IF_ERROR(nearby_platform_Sha256Start());
  RETURN_IF_ERROR(nearby_platform_Sha256Update(hmac_key, HMAC_SHA256_KEY_SIZE));
  RETURN_IF_ERROR(
      nearby_platform_Sha256Update(session_nonce, SESSION_NONCE_SIZE));
  RETURN_IF_ERROR(
      nearby_platform_Sha256Update(message_nonce, SESSION_NONCE_SIZE));
  RETURN_IF_ERROR(nearby_platform_Sha256Update(data, data_length));
  RETURN_IF_ERROR(nearby_platform_Sha256Finish(out));
  return kNearbyStatusOK;
}

nearby_platform_status nearby_fp_HmacSha256Start(
    nearby_fp_HmacSha256Context* context, const uint8_t* key,
    size_t key_length) {
  // out = HASH(Key XOR ipad, data)
  PadKey(context->hmac_key, key, key_length, IPAD);
  RETURN_IF_ERROR(nearby_platform_Sha256Start());
  return nearby_platform_Sha256Update(context->hmac_key, HMAC_SHA256_KEY_SIZE);
}

nearby_platform_status nearby_fp_HmacSha256Update(const uint8_t* data,
                                                  size_t data_length) {
  return nearby_platform_Sha256Update(data, data_length);
}

nearby_platform_status nearby_fp_HmacSha256Finish(
    nearby_fp_HmacSha256Context* context, const uint8_t* key,
    size_t key_length) {
  RETURN_IF_ERROR(nearby_platform_Sha256Finish(context->hash));
  // out = HASH(Key XOR opad, out)
  PadKey(context->hmac_key, key, key_length, OPAD);
  return HmacSha256(context->hash, context->hmac_key, context->hash,
                    SHA256_KEY_SIZE);
}

nearby_platform_status nearby_fp_HmacSha256(uint8_t out[SHA256_KEY_SIZE],
                                            const uint8_t* key,
                                            size_t key_length,
                                            const uint8_t* data,
                                            size_t data_length) {
  uint8_t hmac_key[HMAC_SHA256_KEY_SIZE];
  // out = HASH(Key XOR ipad, data)
  PadKey(hmac_key, key, key_length, IPAD);
  RETURN_IF_ERROR(HmacSha256(out, hmac_key, data, data_length));
  // out = HASH(Key XOR opad, out)
  PadKey(hmac_key, key, key_length, OPAD);
  return HmacSha256(out, hmac_key, out, SHA256_KEY_SIZE);
}

nearby_platform_status nearby_fp_HkdfExtractSha256(uint8_t out[SHA256_KEY_SIZE],
                                                   const uint8_t* salt,
                                                   size_t salt_length,
                                                   const uint8_t* ikm,
                                                   size_t ikm_length) {
  return nearby_fp_HmacSha256(out, salt, salt_length, ikm, ikm_length);
}

nearby_platform_status nearby_fp_HkdfExpandSha256(
    uint8_t* out, size_t out_length, const uint8_t* prk, size_t prk_length,
    const uint8_t* info, size_t info_length) {
  nearby_fp_HmacSha256Context context;
  size_t offset = 0;
  uint8_t chunk_number = 1;
  while (offset < out_length) {
    RETURN_IF_ERROR(nearby_fp_HmacSha256Start(&context, prk, prk_length));
    if (offset != 0) {
      RETURN_IF_ERROR(
          nearby_fp_HmacSha256Update(context.hash, SHA256_KEY_SIZE));
    }
    RETURN_IF_ERROR(nearby_fp_HmacSha256Update(info, info_length));
    RETURN_IF_ERROR(
        nearby_fp_HmacSha256Update(&chunk_number, sizeof(chunk_number)));
    RETURN_IF_ERROR(nearby_fp_HmacSha256Finish(&context, prk, prk_length));
    size_t space_left = out_length - offset;
    size_t bytes_to_copy =
        space_left < SHA256_KEY_SIZE ? space_left : SHA256_KEY_SIZE;
    memcpy(out + offset, context.hash, bytes_to_copy);
    offset += bytes_to_copy;
    ++chunk_number;
  }
  return kNearbyStatusOK;
}

static nearby_platform_status GetRrdKey(
    uint8_t out[AES_MESSAGE_SIZE_BYTES],
    const uint8_t account_key[ACCOUNT_KEY_SIZE_BYTES]) {
  uint8_t prk[SHA256_KEY_SIZE];
  RETURN_IF_ERROR(nearby_fp_HkdfExtractSha256(prk, NULL, 0, account_key,
                                              ACCOUNT_KEY_SIZE_BYTES));
  return nearby_fp_HkdfExpandSha256(out, AES_MESSAGE_SIZE_BYTES, prk,
                                    sizeof(prk), kSassRrdKey,
                                    sizeof(kSassRrdKey));
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
    unsigned i;
    unsigned bytes_left = message_length - offset;
    RETURN_IF_ERROR(nearby_platform_Aes128Encrypt(iv, key_stream, key));
    for (i = 0; i < bytes_left && i < sizeof(key_stream); i++) {
      message[offset++] ^= key_stream[i];
    }
    iv[0]++;
  }
  return kNearbyStatusOK;
}

#if NEARBY_FP_ENABLE_ADDITIONAL_DATA
#define ADDITIONAL_DATA_SHA_SIZE 8
nearby_platform_status nearby_fp_DecodeAdditionalData(
    uint8_t* data, size_t length, const uint8_t key[ACCOUNT_KEY_SIZE_BYTES]) {
  NEARBY_ASSERT(length > ADDITIONAL_DATA_SHA_SIZE + NONCE_SIZE);
  uint8_t sha[32];
  RETURN_IF_ERROR(nearby_fp_HmacSha256(sha, key, ACCOUNT_KEY_SIZE_BYTES,
                                       data + ADDITIONAL_DATA_SHA_SIZE,
                                       length - ADDITIONAL_DATA_SHA_SIZE));
  if (memcmp(sha, data, ADDITIONAL_DATA_SHA_SIZE) != 0) {
    NEARBY_TRACE(WARNING, "Additional Data SHA check failed");
    NEARBY_TRACE(WARNING, "SHA: %s",
                 nearby_utils_ArrayToString(sha, sizeof(sha)));
    NEARBY_TRACE(
        WARNING, "key: %s",
        nearby_utils_ArrayToString(key, sizeof(ACCOUNT_KEY_SIZE_BYTES)));
    NEARBY_TRACE(WARNING, "data: %s", nearby_utils_ArrayToString(data, length));
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

uint8_t nearby_fp_GetSassConnectionState() {
  return (BOOL_TO_INT(nearby_platform_OnHead())
          << SASS_CONN_STATE_ON_HEAD_OFFSET) |
         (BOOL_TO_INT(nearby_platform_CanAcceptConnection())
          << SASS_CONN_STATE_AVAIL_OFFSET) |
         (BOOL_TO_INT(nearby_platform_InFocusMode())
          << SASS_CONN_STATE_FOCUS_OFFSET) |
         (BOOL_TO_INT(nearby_platform_AutoReconnected())
          << SASS_CONN_STATE_AUTO_RECONNECTED_OFFSET) |
         (nearby_platform_GetAudioConnectionState() & SASS_CONN_STATE_MASK);
}

size_t nearby_fp_GenerateSassAdvertisement(
    uint8_t* advertisement, size_t length, uint8_t connection_state,
    uint8_t custom_data, const uint8_t* devices_bitmap, size_t bitmap_length) {
  size_t advert_size = MIN_SASS_ADERTISEMENT_SIZE + bitmap_length;
  NEARBY_ASSERT(length >= advert_size);
  size_t offset = 0;
  advertisement[offset++] = combineNibbles(advert_size - SASS_HEADER_SIZE,
                                           SASS_ADVERTISEMENT_FIELD_TYPE);
  advertisement[offset++] = connection_state;
  advertisement[offset++] = custom_data;
  memcpy(advertisement + offset, devices_bitmap, bitmap_length);
  return advert_size;
}

nearby_platform_status nearby_fp_EncryptRandomResolvableField(
    uint8_t* data, size_t length, const uint8_t key[AES_MESSAGE_SIZE_BYTES],
    const uint8_t* salt_field) {
  NEARBY_ASSERT(length <= AES_MESSAGE_SIZE_BYTES + RRF_HEADER_SIZE);
  uint8_t iv[AES_MESSAGE_SIZE_BYTES];
  uint8_t rrd_key[AES_MESSAGE_SIZE_BYTES];
  memset(iv, 0, sizeof(iv));
  if (salt_field != NULL) {
    int salt_length = GetLtLength(salt_field[0]);
    memcpy(iv, salt_field + LTV_HEADER_SIZE, salt_length);
  }
  RETURN_IF_ERROR(GetRrdKey(rrd_key, key));
  RETURN_IF_ERROR(nearby_platform_Aes128Encrypt(iv, iv, rrd_key));
  data[0] =
      combineNibbles(length - RRF_HEADER_SIZE, RANDOM_RESOLVABLE_FIELD_TYPE);
  for (size_t i = RRF_HEADER_SIZE; i < length; i++) {
    data[i] ^= iv[i - RRF_HEADER_SIZE];
  }
  return kNearbyStatusOK;
}

nearby_platform_status nearby_fp_AesEncryptIv(
    uint8_t* data, size_t length, uint8_t iv[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]) {
  NEARBY_ASSERT(length <= AES_MESSAGE_SIZE_BYTES);
  RETURN_IF_ERROR(nearby_platform_Aes128Encrypt(iv, iv, key));
  for (size_t i = 0; i < length; i++) {
    data[i] ^= iv[i];
  }
  return kNearbyStatusOK;
}

uint16_t nearby_fp_GetSassCapabilityFlags() {
  return (BOOL_TO_INT(nearby_platform_IsSassOn()) << SASS_CF_ON_OFFSET) |
         (BOOL_TO_INT(nearby_platform_IsMultipointConfigurable())
          << SASS_CF_MULTIPOINT_CONFIGURABLE) |
         (BOOL_TO_INT(nearby_platform_IsMultipointOn())
          << SASS_CF_MULTIPOINT_ON) |
         (BOOL_TO_INT(nearby_platform_IsOnHeadDetectionSupported())
          << SASS_CF_OHD_SUPPORTED) |
         (BOOL_TO_INT(nearby_platform_IsOnHeadDetectionEnabled())
          << SASS_CF_OHD_ENABLED);
}

nearby_platform_status nearby_fp_VerifyMessageAuthenticationCode(
    const uint8_t* message, size_t length,
    const uint8_t key[ACCOUNT_KEY_SIZE_BYTES],
    const uint8_t session_nonce[SESSION_NONCE_SIZE]) {
  if (length <= SESSION_NONCE_SIZE - MESSAGE_AUTHENTICATION_CODE_SIZE) {
    return kNearbyStatusInvalidInput;
  }
  size_t data_length =
      length - SESSION_NONCE_SIZE - MESSAGE_AUTHENTICATION_CODE_SIZE;
  const uint8_t* message_nonce = message + data_length;
  uint8_t hmac_key[HMAC_SHA256_KEY_SIZE];
  uint8_t out[32];
  // out = HASH(Key XOR ipad, session nonce + message nonce + data)
  PadKey(hmac_key, key, ACCOUNT_KEY_SIZE_BYTES, IPAD);
  RETURN_IF_ERROR(HmacSha256WithNonce(out, hmac_key, session_nonce,
                                      message_nonce, message, data_length));
  // out = HASH(Key XOR opad, out)
  PadKey(hmac_key, key, ACCOUNT_KEY_SIZE_BYTES, OPAD);
  RETURN_IF_ERROR(HmacSha256(out, hmac_key, out, 32));
  const uint8_t* mac = message + data_length + SESSION_NONCE_SIZE;
  if (memcmp(out, mac, MESSAGE_AUTHENTICATION_CODE_SIZE)) {
    NEARBY_TRACE(INFO,
                 "Expected MAC "
                 "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x",
                 out[0], out[1], out[2], out[3], out[4], out[5], out[6],
                 out[7]);
    return kNearbyStatusInvalidInput;
  } else {
    return kNearbyStatusOK;
  }
}
