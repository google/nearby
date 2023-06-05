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

// clang-format off
#include "nearby_config.h"
// clang-format on

#include "nearby_spot.h"

#include "nearby.h"
#include "nearby_advert.h"
#include "nearby_assert.h"
#include "nearby_fp_library.h"
#include "nearby_platform_ble.h"
#include "nearby_platform_bt.h"
#include "nearby_platform_os.h"
#include "nearby_platform_persistence.h"
#include "nearby_platform_se.h"
#include "nearby_trace.h"
#include "nearby_utils.h"
#ifdef NEARBY_SPOT_BATTERY_LEVEL_INDICATION
#include "nearby_platform_battery.h"
#endif

#if NEARBY_FP_ENABLE_SPOT

#define STATUS_UNAUTHENTICATED 0x80
#define STATUS_INVALID_VALUE 0x81
#define STATUS_NO_USER_CONSENT 0x82
#define BEACON_NONCE_SIZE 8

#define REQUEST_READ_BEACON_PARAMETERS 0x00
#define REQUEST_READ_PROVISIONING_STATE 0x01
#define REQUEST_SET_EPHEMERAL_KEY 0x02
#define REQUEST_CLEAR_EPHEMERAL_KEY 0x03
#define REQUEST_READ_EPHEMERAL_KEY 0x04
#define REQUEST_RING 0x05
#define REQUEST_READ_RINGING_STATE 0x06
#define REQUEST_ACTIVATE_UNWANTED_TRACKING_PROTECTION 0x07
#define REQUEST_DEACTIVATE_UNWANTED_TRACKING_PROTECTION 0x08

#define RESPONSE_READ_BEACON_PARAMETERS 0x00
#define RESPONSE_READ_PROVISIONING_STATE 0x01
#define RESPONSE_SET_EPHEMERAL_KEY 0x02
#define RESPONSE_CLEAR_EPHEMERAL_KEY 0x03
#define RESPONSE_READ_EPHEMERAL_KEY 0x04
#define RESPONSE_RING_STATE_CHANGE 0x05
#define RESPONSE_READ_RINGING_STATE 0x06
#define RESPONSE_ACTIVATE_UNWANTED_TRACKING_PROTECTION 0x07
#define RESPONSE_DEACTIVATE_UNWANTED_TRACKING_PROTECTION 0x08

#define FRAME_TYPE_SIZE 1
#define FRAME_TYPE 0x40
#define FRAME_TYPE_WITH_UNWANTED_TRACKING_PROTECTION 0x41

#define REQUEST_HEADER_SIZE 2
#define RESPONSE_HEADER_SIZE 2
// The size of authentication key in the write requests
#define AUTH_KEY_SIZE 8
#define EPHEMERAL_KEY_SIZE 32
#define RECOVERY_KEY_SUFFIX 0x01
#define RING_KEY_SUFFIX 0x02
#define UNWANTED_TRACKING_PROTECTION_KEY_SUFFIX 0x03
// The size of `ring` request data.
#define RING_STATE_SIZE 3

#define ROTATION_PERIOD_EXPONENT 10
#define CLOCK_MASK (~((1 << (ROTATION_PERIOD_EXPONENT)) - 1))

#define BEACON_PROTOCOL_VERSION_SIZE 1

#ifdef NEARBY_SPOT_BATTERY_LEVEL_INDICATION
#define BATTERY_LEVEL_UNSUPPORTED_MASK 0x00
#define BATTERY_LEVEL_NORMAL_MASK 0x02
#define BATTERY_LEVEL_LOW_MASK 0x04
#define BATTERY_LEVEL_CRITICALLY_LOW_MASK 0x06
#endif /* NEARBY_SPOT_BATTERY_LEVEL_INDICATION */

#define CONTROL_FLAG_SKIP_RINGING_AUTHENTICATION 0x01

static const uint8_t frame_header[] = {2, 1, 6, 24, 0x16, 0xAA, 0xFE};
static uint64_t remote_address;
static bool has_nonce;
static uint8_t nonce[BEACON_NONCE_SIZE];
static bool has_owner_key;
static uint8_t owner_key[ACCOUNT_KEY_SIZE_BYTES];
static bool has_ephemeral_key;
static uint8_t ephemeral_key[EPHEMERAL_KEY_SIZE];
static uint8_t ephemeral_id[EPHEMERAL_ID_SIZE];
static uint8_t hashed_field_lsb;
static uint8_t unwanted_tracking_protection_mode = 0;
uint8_t account_key_index = 0xFF;
static uint8_t control_flags = 0x00;
static uint64_t ringing_peer_address = 0x00;
#if NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES
static uint64_t spot_randomized_address = 0;
static unsigned int spot_address_rotation_timestamp = 0;
#endif /* NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES */

#define RETURN_IF_ERROR(X)                        \
  do {                                            \
    nearby_platform_status status = X;            \
    if (kNearbyStatusOK != status) return status; \
  } while (0)

#ifdef NEARBY_SPOT_BATTERY_LEVEL_INDICATION
static uint8_t nearby_spot_GetBatteryLevel(void) {
#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
  nearby_platform_status status;
  nearby_platform_BatteryInfo battery_info;
  // set to incorrect battery level
  battery_info.battery_level[MAIN_BATTERY_LEVEL_INDEX] = 0xFF;
  status = nearby_platform_GetBatteryInfo(&battery_info);

  // Read the battery level (lower 7 bits)
  uint8_t main_battery_lev = NEARBY_PLATFORM_GET_BATTERY_LEVEL(
      battery_info.battery_level[MAIN_BATTERY_LEVEL_INDEX]);

  if ((status == kNearbyStatusOK) && (main_battery_lev != 0xFF)) {
    if (main_battery_lev <= NEARBY_SPOT_BATTERY_LEVEL_CRITICALLY_LOW_THRESHOLD)
      return BATTERY_LEVEL_CRITICALLY_LOW_MASK;
    else if (main_battery_lev <= NEARBY_SPOT_BATTERY_LEVEL_LOW_THRESHOLD)
      return BATTERY_LEVEL_LOW_MASK;
    else
      return BATTERY_LEVEL_NORMAL_MASK;
  }
#endif
  return BATTERY_LEVEL_UNSUPPORTED_MASK;
}

#endif  // NEARBY_SPOT_BATTERY_LEVEL_INDICATION

void nearby_spot_SetUnwantedTrackingProtectionMode(bool activate) {
  if (activate)
    unwanted_tracking_protection_mode = 0x01;
  else
    unwanted_tracking_protection_mode = 0x00;
}

bool nearby_spot_GetUnwantedTrackingProtectionModeState(void) {
  return unwanted_tracking_protection_mode;
}

uint8_t nearby_spot_GetControlFlags(void) { return control_flags; }

nearby_platform_status nearby_spot_GetEid(uint8_t* eid_ptr) {
  if (has_ephemeral_key && eid_ptr != NULL)
    memcpy(eid_ptr, &ephemeral_id, EPHEMERAL_ID_SIZE);
  else
    return kNearbyStatusInvalidInput;
  return kNearbyStatusOK;
}

// Returns true if the `key` matches.
static bool VerifyKey(const uint8_t* key, size_t key_size,
                      const uint8_t* auth_key) {
  uint8_t hash[32];
  RETURN_IF_ERROR(nearby_platform_Sha256Start());
  RETURN_IF_ERROR(nearby_platform_Sha256Update(key, key_size));
  RETURN_IF_ERROR(nearby_platform_Sha256Update(nonce, sizeof(nonce)));
  RETURN_IF_ERROR(nearby_platform_Sha256Finish(hash));
  bool result = memcmp(hash, auth_key, AUTH_KEY_SIZE) == 0;
  if (!result) {
    NEARBY_TRACE(DEBUG, "Auth key: %s",
                 nearby_utils_ArrayToString(auth_key, AUTH_KEY_SIZE));
    NEARBY_TRACE(DEBUG, "Key: %s", nearby_utils_ArrayToString(key, key_size));
    NEARBY_TRACE(DEBUG, "Nonce: %s",
                 nearby_utils_ArrayToString(nonce, sizeof(nonce)));
  }
  NEARBY_TRACE(DEBUG, "VerifyKey: %s", result ? "OK" : "failed");
  return result;
}

// The function verifies or generate the auth key and returns ture
// auth_key = HMAC-SHA256(key, major protocol version || nonce || data ID
// || data lenght || additional data)
static bool VerifyOrGenerateAuthenticationKey(const uint8_t* key,
                                              size_t key_size,
                                              uint8_t* auth_key,
                                              const uint8_t* payload,
                                              bool generate) {
  nearby_fp_HmacSha256Context context;
  uint8_t protocol_major_version = SPOT_BEACON_PROTOCOL_MAJOR_VERSION;
  uint8_t additional_data_length = (payload[1] - AUTH_KEY_SIZE);

  // The first 8 bytes of HMAC-SHA256(Account Key,
  // Protocol major version number || the last nonce read from the
  // characteristic|| Data ID ||Data length || Additional Data).
  RETURN_IF_ERROR(nearby_fp_HmacSha256Start(&context, key, key_size));
  RETURN_IF_ERROR(
      nearby_fp_HmacSha256Update(&protocol_major_version, sizeof(uint8_t)));

  RETURN_IF_ERROR(nearby_fp_HmacSha256Update(nonce, sizeof(nonce)));
  // Data ID which is first byte in payload & Data length : second byte in
  // payload
  RETURN_IF_ERROR(nearby_fp_HmacSha256Update(&payload[0], 2 * sizeof(uint8_t)));
  // Check if addtioal data is present or not
  if (additional_data_length) {
    RETURN_IF_ERROR(nearby_fp_HmacSha256Update(
        &payload[REQUEST_HEADER_SIZE + AUTH_KEY_SIZE], additional_data_length));
  }

  // For generating the auth key for response, also append 0x01 at the end
  if (generate) {
    uint8_t last_hmac_data = 0x01;
    RETURN_IF_ERROR(
        nearby_fp_HmacSha256Update(&last_hmac_data, sizeof(uint8_t)));
  }
  RETURN_IF_ERROR(nearby_fp_HmacSha256Finish(&context, key, key_size));
  bool result = true;
  if (generate) {
    memcpy(auth_key, context.hash, AUTH_KEY_SIZE);
  } else {
    result = memcmp(context.hash, auth_key, AUTH_KEY_SIZE) == 0;
    NEARBY_TRACE(DEBUG, "Hash: %s",
                 nearby_utils_ArrayToString(context.hash, AUTH_KEY_SIZE));
    if (!result) {
      NEARBY_TRACE(DEBUG, "Auth key: %s",
                   nearby_utils_ArrayToString(auth_key, AUTH_KEY_SIZE));
      NEARBY_TRACE(DEBUG, "Key: %s", nearby_utils_ArrayToString(key, key_size));
      NEARBY_TRACE(DEBUG, "Nonce: %s",
                   nearby_utils_ArrayToString(nonce, sizeof(nonce)));
      NEARBY_TRACE(DEBUG, "VerifyOrGenerateAuthenticationKey: %s",
                   result ? "OK" : "failed");
    }
  }
  return result;
}

static bool VerifyAnyAccountKey(uint8_t* auth_key, const uint8_t* request) {
  for (size_t i = 0; i < nearby_fp_GetAccountKeyCount(); i++) {
    if (VerifyOrGenerateAuthenticationKey(
            nearby_fp_GetAccountKey(i)->account_key, ACCOUNT_KEY_SIZE_BYTES,
            auth_key, request, false)) {
      account_key_index = i;
      return true;
    }
  }
  return false;
}

#define VERIFY_KEY(key, auth_key)                   \
  if (!VerifyKey(key, sizeof(key), auth_key)) {     \
    NEARBY_TRACE(ERROR, "Failed Key verification"); \
    return STATUS_UNAUTHENTICATED;                  \
  }

#define VERIFY_AUTH_KEY(key, auth_key, request)                               \
  if (!VerifyOrGenerateAuthenticationKey(key, sizeof(key), auth_key, request, \
                                         false)) {                            \
    NEARBY_TRACE(ERROR, "Failed auth");                                       \
    return STATUS_UNAUTHENTICATED;                                            \
  }

#define GENERATE_AUTH_KEY(key, key_size, auth_key, payload)                \
  if (!VerifyOrGenerateAuthenticationKey(key, key_size, auth_key, payload, \
                                         true)) {                          \
    NEARBY_TRACE(ERROR, "Failed auth");                                    \
    return STATUS_UNAUTHENTICATED;                                         \
  }

#define VERIFY_AUTH_KEY_WITH_ANY_ACCOUNT_KEY(auth_key, request) \
  if (!VerifyAnyAccountKey(auth_key, request)) {                \
    NEARBY_TRACE(ERROR, "Failed verify any account key");       \
    return STATUS_UNAUTHENTICATED;                              \
  }

#define VERIFY_COND(cond)                  \
  if (!(cond)) {                           \
    NEARBY_TRACE(ERROR, "Failed: " #cond); \
    return STATUS_INVALID_VALUE;           \
  }

static bool HasUserConsentForReadingEik() {
  return (nearby_platform_IsInPairingMode() ||
          nearby_platform_HasUserConsentForReadingEik());
}
static nearby_platform_status ComputeKey(uint8_t output[AUTH_KEY_SIZE],
                                         uint8_t suffix) {
  uint8_t hash[32];
  NEARBY_ASSERT(has_ephemeral_key);
  RETURN_IF_ERROR(nearby_platform_Sha256Start());
  RETURN_IF_ERROR(
      nearby_platform_Sha256Update(ephemeral_key, sizeof(ephemeral_key)));
  RETURN_IF_ERROR(nearby_platform_Sha256Update(&suffix, sizeof(suffix)));
  RETURN_IF_ERROR(nearby_platform_Sha256Finish(hash));
  memcpy(output, hash, AUTH_KEY_SIZE);
  return kNearbyStatusOK;
}

static nearby_platform_status ComputeRecoveryKey(
    uint8_t output[AUTH_KEY_SIZE]) {
  return ComputeKey(output, RECOVERY_KEY_SUFFIX);
}

static nearby_platform_status ComputeRingKey(uint8_t output[AUTH_KEY_SIZE]) {
  return ComputeKey(output, RING_KEY_SUFFIX);
}

static nearby_platform_status ComputeUnwantedTrackingProtectionKey(
    uint8_t output[AUTH_KEY_SIZE]) {
  return ComputeKey(output, UNWANTED_TRACKING_PROTECTION_KEY_SUFFIX);
}

static nearby_platform_status GattNotify(
    uint64_t address, nearby_fp_Characteristic characteristic,
    const uint8_t* message, size_t length) {
  nearby_platform_status status =
      nearby_platform_GattNotify(address, characteristic, message, length);
  NEARBY_TRACE(DEBUG, "Notify(%s), result %d",
               nearby_utils_ArrayToString(message, length), status);
  return status;
}
// TODO(jsobczak): The Owner Account Key must not be removed when the Provider
// runs out of free Account Key slots.
nearby_platform_status nearby_spot_SetBeaconAccountKey(
    const uint8_t key[ACCOUNT_KEY_SIZE_BYTES]) {
  NEARBY_TRACE(VERBOSE, "SetBeaconAccountKey");
  memcpy(owner_key, key, sizeof(owner_key));
  has_owner_key = true;
  return nearby_platform_SaveValue(kStoredKeyOwnerKey, owner_key,
                                   sizeof(owner_key));
}

nearby_platform_status nearby_spot_ReadBeaconAction(uint64_t peer_address,
                                                    uint8_t* output,
                                                    size_t* length) {
  NEARBY_ASSERT(*length >= (BEACON_NONCE_SIZE + BEACON_PROTOCOL_VERSION_SIZE));
  *length = BEACON_NONCE_SIZE + BEACON_PROTOCOL_VERSION_SIZE;
  remote_address = peer_address;
  output[0] = SPOT_BEACON_PROTOCOL_MAJOR_VERSION;
  for (int i = 0; i < BEACON_NONCE_SIZE; i++) {
    nonce[i] = output[i + 1] = nearby_platform_Rand();
  }
  has_nonce = true;
  return kNearbyStatusOK;
}

static nearby_platform_status GenerateEphemeralId() {
  uint8_t buffer[EPHEMERAL_KEY_SIZE];
  uint32_t timestamp = nearby_platform_GetPersistentTime() & CLOCK_MASK;
  NEARBY_TRACE(INFO, "Timestamp 0x%x", timestamp);
  // Bytes 0 - 10: padding 0xFF
  memset(buffer, 0xFF, 11);
  buffer[11] = ROTATION_PERIOD_EXPONENT;
  nearby_utils_CopyBigEndian(buffer + 12, timestamp, sizeof(timestamp));
  // Bytes 16 - 26: padding 0x00
  memset(buffer + 16, 0, 11);
  buffer[27] = ROTATION_PERIOD_EXPONENT;
  memcpy(buffer + 28, buffer + 12, sizeof(timestamp));
  NEARBY_TRACE(
      INFO, "EIK %s",
      nearby_utils_ArrayToString(ephemeral_key, sizeof(ephemeral_key)));
  NEARBY_TRACE(INFO, "EID enc input %s",
               nearby_utils_ArrayToString(buffer, sizeof(buffer)));
  RETURN_IF_ERROR(nearby_platform_Aes256Encrypt(buffer, buffer, ephemeral_key));
  RETURN_IF_ERROR(
      nearby_platform_Aes256Encrypt(buffer + 16, buffer + 16, ephemeral_key));
  NEARBY_TRACE(INFO, "EID enc output %s",
               nearby_utils_ArrayToString(buffer, sizeof(buffer)));
  nearby_platform_status status = nearby_platform_GetSecp160r1PublicKey(
      buffer, ephemeral_id, &hashed_field_lsb);
  NEARBY_TRACE(INFO, "EID %s",
               nearby_utils_ArrayToString(ephemeral_id, sizeof(ephemeral_id)));
  return status;
}

#if NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES
unsigned int nearby_spot_GetAddressRotationTimestamp() {
  return spot_address_rotation_timestamp;
}

void nearby_spot_GenerateRandomizedAddress() {
  uint64_t address = 0;
  for (int i = 0; i < BT_ADDRESS_LENGTH; i++) {
    address = (address << 8) ^ nearby_platform_Rand();
  }
  address &= ~((uint64_t)3 << 46);
  spot_randomized_address = address;
  spot_address_rotation_timestamp = nearby_platform_GetCurrentTimeMs();
  NEARBY_TRACE(VERBOSE, "address=0x%lx spot_address_rotation_timestamp=%u",
               address, spot_address_rotation_timestamp);
}

#endif /* NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES */

static uint64_t GetSpotAddress() {
#if NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES
  if (nearby_spot_GetUnwantedTrackingProtectionModeState()) {
    return spot_randomized_address;
  }
#endif /* NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES*/
  return nearby_platform_GetBleAddress();
}

static nearby_platform_status StartAdvertising() {
  NEARBY_TRACE(VERBOSE, "Start SPOT advertisement");
  uint8_t hashed_flag = 0;
  uint64_t address = GetSpotAddress();
  uint8_t advertisement[sizeof(frame_header) + FRAME_TYPE_SIZE +
                        sizeof(ephemeral_id) + sizeof(hashed_flag)];
  uint8_t adv_length = sizeof(advertisement);
  memcpy(advertisement, frame_header, sizeof(frame_header));
  if (nearby_spot_GetUnwantedTrackingProtectionModeState())
    advertisement[sizeof(frame_header)] =
        FRAME_TYPE_WITH_UNWANTED_TRACKING_PROTECTION;
  else
    advertisement[sizeof(frame_header)] = FRAME_TYPE;
  memcpy(advertisement + sizeof(frame_header) + FRAME_TYPE_SIZE, ephemeral_id,
         sizeof(ephemeral_id));

  hashed_flag |= unwanted_tracking_protection_mode;
#ifdef NEARBY_SPOT_BATTERY_LEVEL_INDICATION
  hashed_flag |= nearby_spot_GetBatteryLevel();
#endif
  if (hashed_flag) {
    hashed_flag ^= hashed_field_lsb;
    memcpy(advertisement + sizeof(frame_header) + FRAME_TYPE_SIZE +
               sizeof(ephemeral_id),
           &hashed_flag, sizeof(hashed_flag));
  } else
    adv_length -= 1;

  NEARBY_TRACE(INFO, "SPOT advert %s",
               nearby_utils_ArrayToString(advertisement, adv_length));
  return nearby_SetSpotAdvertisement(address, advertisement, adv_length);
}

static nearby_platform_status StopAdvertising() {
  NEARBY_TRACE(VERBOSE, "Stop SPOT advertisement");
  return nearby_SetSpotAdvertisement(0, NULL, 0);
}

static nearby_platform_status UpdateAdvertisement() {
  nearby_platform_status status = GenerateEphemeralId();
  StopAdvertising();
  if (kNearbyStatusOK == status) {
    status = StartAdvertising();
  }
  return status;
}
static nearby_platform_status ReadBeaconParameters(uint64_t peer_address) {
  NEARBY_TRACE(VERBOSE, "ReadBeaconParameters");
  uint8_t payload[26] = {0};
  uint8_t* additional_data = &payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE];
  nearby_platform_RingingInfo info;

  payload[0] = RESPONSE_READ_BEACON_PARAMETERS;
  payload[1] = sizeof(payload) - RESPONSE_HEADER_SIZE;

  additional_data[0] = nearby_platform_GetTxLevel();
  nearby_utils_CopyBigEndian(&additional_data[1],
                             nearby_platform_GetPersistentTime(),
                             sizeof(uint32_t));
  // curve type
#if (NEARBY_SPOT_SECP_CURVE_SIZE == 20)
  additional_data[5] = 0x00;
#else
  additional_data[5] = 0x01;
#endif
  RETURN_IF_ERROR(nearby_platform_GetRingingInfo(&info));
  // number of ringing component
  additional_data[6] = info.num_components;
  // ringing volume capability
  additional_data[7] = info.volume;
  // zero padded 8 bytes
  memset(&additional_data[8], 0, 8);

  if (account_key_index != 0xFF) {
    // Encrypt the data
    const uint8_t* key =
        nearby_fp_GetAccountKey(account_key_index)->account_key;
    RETURN_IF_ERROR(nearby_platform_Aes128Encrypt(
        additional_data, &payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE], key));
    // Generate and the set the authentication key
    GENERATE_AUTH_KEY(key, ACCOUNT_KEY_SIZE_BYTES,
                      &payload[RESPONSE_HEADER_SIZE], payload);
  } else {
    return STATUS_UNAUTHENTICATED;
  }
  return GattNotify(peer_address, kBeaconActions, payload, sizeof(payload));
}

static nearby_platform_status ReadProvisioningState(uint64_t peer_address,
                                                    bool is_owner_key) {
  NEARBY_TRACE(VERBOSE, "ReadProvisioningState");
  uint8_t
      payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE + 1 + sizeof(ephemeral_id)];
  payload[0] = RESPONSE_READ_PROVISIONING_STATE;
  payload[1] = AUTH_KEY_SIZE + 1;
  payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE] =
      (has_ephemeral_key ? 1 : 0) | (is_owner_key ? 2 : 0);
  if (has_ephemeral_key) {
    payload[1] += sizeof(ephemeral_id);
    memcpy(&payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE + 1], ephemeral_id,
           sizeof(ephemeral_id));
  }
  if (account_key_index != 0xFF) {
    // Generate and set the authentication key
    const uint8_t* key =
        nearby_fp_GetAccountKey(account_key_index)->account_key;
    GENERATE_AUTH_KEY(key, ACCOUNT_KEY_SIZE_BYTES,
                      &payload[RESPONSE_HEADER_SIZE], payload);
  } else {
    return STATUS_UNAUTHENTICATED;
  }
  return GattNotify(peer_address, kBeaconActions, payload,
                    RESPONSE_HEADER_SIZE + payload[1]);
}

static nearby_platform_status DecryptAndSaveEphemeralKey(
    const uint8_t* encrypted_key) {
  NEARBY_TRACE(VERBOSE, "Decrypt and save ephemeral key");
  RETURN_IF_ERROR(
      nearby_platform_Aes128Decrypt(encrypted_key, ephemeral_key, owner_key));
  RETURN_IF_ERROR(nearby_platform_Aes128Decrypt(encrypted_key + 16,
                                                ephemeral_key + 16, owner_key));
  RETURN_IF_ERROR(nearby_platform_SaveValue(
      kStoredKeyEphemeralKey, ephemeral_key, sizeof(ephemeral_key)));
  has_ephemeral_key = true;
  return UpdateAdvertisement();
}

static nearby_platform_status SetEphemeralIdentityKeyResponse(
    uint64_t peer_address) {
  NEARBY_TRACE(VERBOSE, "SetEphemeralIdentityKeyResponse");
  uint8_t payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE];
  payload[0] = RESPONSE_SET_EPHEMERAL_KEY;
  payload[1] = AUTH_KEY_SIZE;

  // Generate and set the authentication key
  GENERATE_AUTH_KEY(owner_key, ACCOUNT_KEY_SIZE_BYTES,
                    &payload[RESPONSE_HEADER_SIZE], payload);
  return GattNotify(peer_address, kBeaconActions, payload,
                    RESPONSE_HEADER_SIZE + payload[1]);
}

static nearby_platform_status ClearEphemeralKey() {
  NEARBY_TRACE(VERBOSE, "Clear ephemeral key");
  has_ephemeral_key = false;
  memset(ephemeral_key, 0, sizeof(ephemeral_key));
  memset(ephemeral_id, 0, sizeof(ephemeral_id));
  nearby_platform_status status =
      nearby_platform_ClearValue(kStoredKeyEphemeralKey);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(ERROR, "Error clearing ephemeral key %d", status);
  }
  return StopAdvertising();
}

static nearby_platform_status ClearEphemeralIdentityKeyResponse(
    uint64_t peer_address) {
  NEARBY_TRACE(VERBOSE, "ClearEphemeralIdentityKeyResponse");
  uint8_t payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE];
  payload[0] = RESPONSE_CLEAR_EPHEMERAL_KEY;
  payload[1] = AUTH_KEY_SIZE;

  nearby_platform_status status = ClearEphemeralKey();
  if (status != kNearbyStatusOK) return status;

  // Generate and set the authentication key
  GENERATE_AUTH_KEY(owner_key, ACCOUNT_KEY_SIZE_BYTES,
                    &payload[RESPONSE_HEADER_SIZE], payload);

  status = GattNotify(peer_address, kBeaconActions, payload,
                      RESPONSE_HEADER_SIZE + payload[1]);
  if (status != kNearbyStatusOK) return status;

#if NEARBY_SPOT_FACTORY_RESET_DEVICE_ON_CLEARING_EIK
  status = nearby_platform_FactoryReset();
#endif

  return status;
}

static nearby_platform_status ReadEphemeralKey(uint64_t peer_address,
                                               uint8_t* recovery_key) {
  NEARBY_TRACE(VERBOSE, "ReadEphemeralKey");
  uint8_t payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE + EPHEMERAL_KEY_SIZE];
  payload[0] = RESPONSE_READ_EPHEMERAL_KEY;
  payload[1] = sizeof(payload) - RESPONSE_HEADER_SIZE;
  // Encrypt the ephemeral key with the account key
  RETURN_IF_ERROR(nearby_platform_Aes128Encrypt(
      ephemeral_key, payload + AUTH_KEY_SIZE + RESPONSE_HEADER_SIZE,
      owner_key));
  RETURN_IF_ERROR(nearby_platform_Aes128Encrypt(
      ephemeral_key + 16, payload + AUTH_KEY_SIZE + RESPONSE_HEADER_SIZE + 16,
      owner_key));
  // Generate and set the authentication key
  GENERATE_AUTH_KEY(recovery_key, AUTH_KEY_SIZE, &payload[RESPONSE_HEADER_SIZE],
                    payload);
  return GattNotify(peer_address, kBeaconActions, payload, sizeof(payload));
}

static nearby_platform_status GetPlaftormRingState(
    nearby_platform_RingingInfo* info) {
  memset(info, 0, sizeof(nearby_platform_RingingInfo));
  RETURN_IF_ERROR(nearby_platform_GetRingingInfo(info));
  if (info->ring_state != kRingStateStarted || info->num_components == 0 ||
      info->components == 0) {
    info->timeout = 0;
  }
  return kNearbyStatusOK;
}

static nearby_platform_status Ring(uint64_t peer_address, const uint8_t* data,
                                   size_t length) {
  NEARBY_TRACE(VERBOSE, "Ring");
  if (length < 1) return STATUS_INVALID_VALUE;
  uint8_t command = data[0];
  uint16_t timeout = 0;
  nearby_platform_RingingVolume volume = kRingingVolumeDefault;
  if (command != 0) {
    if (length != 4) return STATUS_INVALID_VALUE;
    timeout = nearby_utils_GetBigEndian16(&data[1]);
    volume = data[3];
  }
  ringing_peer_address = peer_address;
  return nearby_platform_Ring(command, timeout, volume);
}

nearby_platform_status nearby_spot_OnRingStateChange() {
  uint8_t payload[14];
  nearby_platform_RingingInfo info;
  uint8_t ring_key[AUTH_KEY_SIZE];
  RETURN_IF_ERROR(GetPlaftormRingState(&info));
  VERIFY_COND(has_ephemeral_key);
  RETURN_IF_ERROR(ComputeRingKey(ring_key));

  payload[0] = RESPONSE_RING_STATE_CHANGE;
  payload[1] = sizeof(payload) - RESPONSE_HEADER_SIZE;
  payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE] = info.ring_state;
  payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE + 1] = info.components;
  nearby_utils_CopyBigEndian(payload + RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE + 2,
                             info.timeout, 2);

  // Generate and set the authentication key
  GENERATE_AUTH_KEY(ring_key, AUTH_KEY_SIZE, &payload[RESPONSE_HEADER_SIZE],
                    payload);

  return GattNotify(ringing_peer_address, kBeaconActions, payload,
                    sizeof(payload));
}

static nearby_platform_status ReadRingingState(uint64_t peer_address,
                                               uint8_t* ring_key) {
  NEARBY_TRACE(VERBOSE, "ReadRingingState");
  uint8_t payload[13];
  nearby_platform_RingingInfo info;
  RETURN_IF_ERROR(GetPlaftormRingState(&info));

  payload[0] = RESPONSE_READ_RINGING_STATE;
  payload[1] = sizeof(payload) - RESPONSE_HEADER_SIZE;
  payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE] = info.components;
  nearby_utils_CopyBigEndian(payload + RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE + 1,
                             info.timeout, 2);
  // Generate and set the authentication key
  GENERATE_AUTH_KEY(ring_key, AUTH_KEY_SIZE, &payload[RESPONSE_HEADER_SIZE],
                    payload);
  return GattNotify(peer_address, kBeaconActions, payload, sizeof(payload));
}

static nearby_platform_status ActivateUnwantedTrackingProtection(
    uint64_t peer_address, uint8_t* unwanted_tracking_protection_key,
    const uint8_t* data, size_t length) {
  NEARBY_TRACE(VERBOSE, "ActivateUnwantedTrackingProtection");
  // The first byte of the data section defines control flags for the operation.
  if (length == 1) {
    control_flags = data[AUTH_KEY_SIZE];
  }
  // Once Unwanted Tracking Protection mode was activated, the beacon should
  // reduce address(RPA) rotation frequency to once per 24h.
  nearby_spot_SetUnwantedTrackingProtectionMode(true);
  if (UpdateAdvertisement() != kNearbyStatusOK) {
    return kNearbyStatusError;
  }
  uint8_t payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE];
  payload[0] = RESPONSE_ACTIVATE_UNWANTED_TRACKING_PROTECTION;
  payload[1] = sizeof(payload) - RESPONSE_HEADER_SIZE;
  // Generate and set the authentication key
  GENERATE_AUTH_KEY(unwanted_tracking_protection_key, AUTH_KEY_SIZE,
                    &payload[RESPONSE_HEADER_SIZE], payload);
  return GattNotify(peer_address, kBeaconActions, payload, sizeof(payload));
}

static nearby_platform_status DeactivateUnwantedTrackingProtection(
    uint64_t peer_address, uint8_t* unwanted_tracking_protection_key) {
  NEARBY_TRACE(VERBOSE, "DeactivateUnwantedTrackingProtection");
  // Reset control flags
  control_flags = 0;
  // Once Unwanted Tracking Protection deactivated, the beacon should
  // start rotating the MAC address(RPA) at a normal rate again
  nearby_spot_SetUnwantedTrackingProtectionMode(false);
  if (UpdateAdvertisement() != kNearbyStatusOK) {
    return kNearbyStatusError;
  }
  uint8_t payload[RESPONSE_HEADER_SIZE + AUTH_KEY_SIZE];
  payload[0] = RESPONSE_DEACTIVATE_UNWANTED_TRACKING_PROTECTION;
  payload[1] = sizeof(payload) - RESPONSE_HEADER_SIZE;
  // Generate and set the authentication key
  GENERATE_AUTH_KEY(unwanted_tracking_protection_key, AUTH_KEY_SIZE,
                    &payload[RESPONSE_HEADER_SIZE], payload);
  return GattNotify(peer_address, kBeaconActions, payload, sizeof(payload));
}

nearby_platform_status nearby_spot_WriteBeaconAction(uint64_t peer_address,
                                                     const uint8_t* request,
                                                     size_t length) {
  uint8_t expected_auth_key[AUTH_KEY_SIZE];
  nearby_platform_status status;

  NEARBY_TRACE(INFO, "WriteBeaconAction request from(%s) %s",
               nearby_utils_MacToString(peer_address),
               nearby_utils_ArrayToString(request, length));
  if (peer_address != remote_address || !has_nonce) {
    NEARBY_TRACE(INFO, "Invalid remote address or nonce");
    return STATUS_UNAUTHENTICATED;
  }
  // Mark the nonce as used. It is still valid during this write request.
  has_nonce = false;
  if (length < REQUEST_HEADER_SIZE) return STATUS_INVALID_VALUE;
  uint8_t data_id = request[0];
  uint8_t data_length = request[1];
  uint8_t* auth_key = (uint8_t*)&request[REQUEST_HEADER_SIZE];
  VERIFY_COND(data_length + REQUEST_HEADER_SIZE == length);
  switch (data_id) {
    case REQUEST_READ_BEACON_PARAMETERS:
      account_key_index = 0xFF;
      VERIFY_COND(data_length == AUTH_KEY_SIZE);
      VERIFY_AUTH_KEY_WITH_ANY_ACCOUNT_KEY(auth_key, request);
      return ReadBeaconParameters(peer_address);
    case REQUEST_READ_PROVISIONING_STATE:
      account_key_index = 0xFF;
      VERIFY_COND(data_length == AUTH_KEY_SIZE);
      VERIFY_AUTH_KEY_WITH_ANY_ACCOUNT_KEY(auth_key, request);
      // In some cases, where the Provider didn’t previously keep track of the
      // order of AccountKeys as they’re being added - and now being updated to
      // support Finder, it is allowed to define the Owner Account Key as the
      // first Account Key used for reading the provisioning state from the
      // Provider.
      if (!has_owner_key) {
        nearby_spot_SetBeaconAccountKey(
            nearby_fp_GetAccountKey(account_key_index)->account_key);
      }
      return ReadProvisioningState(
          peer_address,
          VerifyOrGenerateAuthenticationKey(owner_key, sizeof(owner_key),
                                            auth_key, request, false));
    case REQUEST_SET_EPHEMERAL_KEY:
      if (data_length == AUTH_KEY_SIZE + EPHEMERAL_KEY_SIZE) {
        VERIFY_COND(!has_ephemeral_key);
      } else if (data_length ==
                 AUTH_KEY_SIZE + EPHEMERAL_KEY_SIZE + AUTH_KEY_SIZE) {
        VERIFY_COND(has_ephemeral_key);
        VERIFY_KEY(
            ephemeral_key,
            &request[REQUEST_HEADER_SIZE + AUTH_KEY_SIZE + EPHEMERAL_KEY_SIZE]);
      } else {
        return STATUS_INVALID_VALUE;
      }
      VERIFY_COND(has_owner_key);
      VERIFY_AUTH_KEY(owner_key, auth_key, request);
      status = DecryptAndSaveEphemeralKey(
          &request[REQUEST_HEADER_SIZE + AUTH_KEY_SIZE]);
      if (status != kNearbyStatusOK) return status;
      return SetEphemeralIdentityKeyResponse(peer_address);
    case REQUEST_CLEAR_EPHEMERAL_KEY:
      VERIFY_COND(data_length == 2 * AUTH_KEY_SIZE);
      VERIFY_COND(has_owner_key);
      VERIFY_COND(has_ephemeral_key);
      VERIFY_AUTH_KEY(owner_key, auth_key, request);
      VERIFY_KEY(ephemeral_key, &request[REQUEST_HEADER_SIZE + AUTH_KEY_SIZE]);
      return ClearEphemeralIdentityKeyResponse(peer_address);
    case REQUEST_READ_EPHEMERAL_KEY:
      VERIFY_COND(data_length == AUTH_KEY_SIZE);
      VERIFY_COND(has_ephemeral_key);
      RETURN_IF_ERROR(ComputeRecoveryKey(expected_auth_key));
      VERIFY_AUTH_KEY(expected_auth_key, auth_key, request);
      if (!HasUserConsentForReadingEik()) return STATUS_NO_USER_CONSENT;
      return ReadEphemeralKey(peer_address, expected_auth_key);
    case REQUEST_RING:
      VERIFY_COND(data_length > AUTH_KEY_SIZE);
      VERIFY_COND(has_ephemeral_key);
      RETURN_IF_ERROR(ComputeRingKey(expected_auth_key));
      if (!(nearby_spot_GetControlFlags() ==
                CONTROL_FLAG_SKIP_RINGING_AUTHENTICATION &&
            nearby_spot_GetUnwantedTrackingProtectionModeState())) {
        VERIFY_AUTH_KEY(expected_auth_key, auth_key, request);
      }
      return Ring(peer_address, &request[REQUEST_HEADER_SIZE + AUTH_KEY_SIZE],
                  (data_length - AUTH_KEY_SIZE));
    case REQUEST_READ_RINGING_STATE:
      VERIFY_COND(data_length == AUTH_KEY_SIZE);
      VERIFY_COND(has_ephemeral_key);
      RETURN_IF_ERROR(ComputeRingKey(expected_auth_key));
      VERIFY_AUTH_KEY(expected_auth_key, auth_key, request);
      return ReadRingingState(peer_address, expected_auth_key);
    case REQUEST_ACTIVATE_UNWANTED_TRACKING_PROTECTION:
      VERIFY_COND(data_length >= AUTH_KEY_SIZE);
      VERIFY_COND(has_ephemeral_key);
      RETURN_IF_ERROR(ComputeUnwantedTrackingProtectionKey(expected_auth_key));
      VERIFY_AUTH_KEY(expected_auth_key, auth_key, request);
      return ActivateUnwantedTrackingProtection(peer_address, expected_auth_key,
                                                &request[REQUEST_HEADER_SIZE],
                                                (data_length - AUTH_KEY_SIZE));
    case REQUEST_DEACTIVATE_UNWANTED_TRACKING_PROTECTION:
      VERIFY_COND(data_length == 2 * AUTH_KEY_SIZE);
      VERIFY_COND(has_ephemeral_key);
      RETURN_IF_ERROR(ComputeUnwantedTrackingProtectionKey(expected_auth_key));
      VERIFY_AUTH_KEY(expected_auth_key, auth_key, request);
      VERIFY_KEY(ephemeral_key, &request[REQUEST_HEADER_SIZE + AUTH_KEY_SIZE]);
      return DeactivateUnwantedTrackingProtection(peer_address,
                                                  expected_auth_key);
  }
  return STATUS_INVALID_VALUE;
}

nearby_platform_status nearby_spot_SetAdvertisement(bool enable) {
  if (!enable) {
    return StopAdvertising();
  }
  NEARBY_TRACE(INFO, "Set spot advertisement");
  if (!has_ephemeral_key) {
    NEARBY_TRACE(WARNING, "Can't advertise SPOT without ephemeral key");
    return kNearbyStatusError;
  }
  // Strictly speaking, the owner acoount key is not required to generate
  // the advertisement but it indicates that the setup is incorrect.
  if (!has_owner_key) {
    NEARBY_TRACE(ERROR, "Can't advertise SPOT without owner account key");
    return kNearbyStatusError;
  }
  return UpdateAdvertisement();
}

nearby_platform_status nearby_spot_Init() {
  size_t key_length = sizeof(ephemeral_key);
  nearby_platform_status status = nearby_platform_LoadValue(
      kStoredKeyEphemeralKey, ephemeral_key, &key_length);
  has_ephemeral_key =
      kNearbyStatusOK == status && key_length == sizeof(ephemeral_key);
  key_length = sizeof(owner_key);
  status =
      nearby_platform_LoadValue(kStoredKeyOwnerKey, owner_key, &key_length);
  has_owner_key = kNearbyStatusOK == status && key_length == sizeof(owner_key);
  if (has_owner_key) {
    NEARBY_TRACE(DEBUG, "Owner key: %s",
                 nearby_utils_ArrayToString(owner_key, sizeof(owner_key)));
  }
  if (has_ephemeral_key) {
    NEARBY_TRACE(
        DEBUG, "EIK: %s",
        nearby_utils_ArrayToString(ephemeral_key, sizeof(ephemeral_key)));
  }
  remote_address = 0;
  has_nonce = false;
  unwanted_tracking_protection_mode = 0;
#if NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES
  nearby_spot_GenerateRandomizedAddress();
#endif /* NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES */
  return kNearbyStatusOK;
}

bool nearby_spot_IsProvisioned() { return has_ephemeral_key; }
#endif /* NEARBY_FP_ENABLE_SPOT */
