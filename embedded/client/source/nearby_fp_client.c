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

#include "nearby_fp_client.h"

#include <string.h>

#include "nearby.h"
#include "nearby_assert.h"
#include "nearby_event.h"
#include "nearby_fp_library.h"
#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
#include "nearby_platform_battery.h"
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
#if NEARBY_FP_MESSAGE_STREAM
#include "nearby_message_stream.h"
#endif /* NEARBY_FP_MESSAGE_STREAM */
#include "nearby_assert.h"
#include "nearby_platform_audio.h"
#include "nearby_platform_ble.h"
#include "nearby_platform_bt.h"
#include "nearby_platform_os.h"
#include "nearby_platform_persistence.h"
#include "nearby_platform_se.h"
#include "nearby_trace.h"
#include "nearby_utils.h"

#define ENCRYPTED_REQUEST_LENGTH 16
#define PUBLIC_KEY_LENGTH 64
#define REQUEST_BT_ADDRESS_OFFSET 2

// block pairing attempts for 5 minutes after 10 failures
#define MAX_PAIRING_FAILURE_COUNT 10
#define REJECT_PAIRING_TIMEOUT_MS (5 * 60 * 1000)
#define PASSKEY_MAX_WAIT_TIME_MS 10000
#define WAIT_FOR_PAIRING_REQUEST_TIME_MS 10000
#define ACCOUNT_KEY_WRITE_TIME_MS 60000
#define RETRO_PAIRING_REQUEST_TIME_MS 60000
// Defines the timeout when waiting for pairing result when the Seeker writes
// the Account Key before/during pairing.
#define WAIT_FOR_PAIRING_RESULT_AFTER_ACCOUNT_KEY_TIME_MS 60000
#define KEY_BASED_PAIRING_REQUEST_FLAG 0x00
#define ACTION_REQUEST_FLAG 0x10

// KBPR - Key Based Pairing Request
#define KBPR_INITIATE_PAIRING_MASK (1 << 6)
#define KBPR_NOTIFY_EXISTING_NAME_MASK (1 << 5)
#define KBPR_RETROACTIVELY_WRITE_ACCOUNT_KEY_MASK (1 << 4)
#define KBPR_SEEKER_SUPPORTS_BLE_DEVICES_MASK (1 << 3)
#define KBPR_SEEKER_ADDRESS_OFFSET 8

#define ACTION_REQUEST_DEVICE_ACTION_MASK (1 << 7)
#define ACTION_REQUEST_WILL_WRITE_DATA_CHARACTERISTIC_MASK (1 << 6)

#define SEEKER_PASSKEY_MESSAGE_TYPE 0x02
#define PROVIDER_PASSKEY_MESSAGE_TYPE 0x03
#define ACCOUNT_KEY_WRITE_MESSAGE_TYPE 0x04

#define PERSONALIZED_NAME_DATA_ID 1

#define INVALID_BATTERY_LEVEL (-1)

#define INVALID_PEER_ADDRESS 0

// One byte per left, right and charging case
#define BATTERY_LEVELS_SIZE 3

// Size of battery remaining time (16 bits)
#define BATTERY_TIME_SIZE 2

// Size of 'Message Stream PSM' Gatt response.
#define PSM_MESSAGE_LENGTH 3

// The BLE address should be rotated on average every 1024 seconds
#define ADDRESS_ROTATION_PERIOD_MS 1024000

// The maximum size of device bitmap. We require one bit per paired device
#define MAX_DEVICE_BITMAP_SIZE 4

// The maximum payload size for 'Notify connection status' message
#define MAX_NOTIFY_CONNECTION_STATUS_MESSAGE_SIZE \
  (3 + MAX_DEVICE_BITMAP_SIZE + SESSION_NONCE_SIZE)

// Possible values of 'Active device flag'
#define SEEKER_IS_NOT_ACTIVE_DEVICE 0
#define SEEKER_IS_ACTIVE_DEVICE 1
#define SEEKER_IS_NON_SASS 2

#define SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT_NAME_OFFSET 2
#define SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT_TARGET_THIS_DEVICE 1
#define SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT_TARGET_ANOTHER_DEVICE 2

enum PairingState {
  kPairingStateIdle,
  kPairingStateWaitingForPairingRequest,
  kPairingStateWaitingForPasskey,
  kPairingStateWaitingForPairingResult,
  kPairingStateWaitingForAccountKeyWrite,
  kPairingStateWaitingForAdditionalData
} pairing_state;
static const nearby_fp_client_Callbacks* client_callbacks;
static unsigned int timeout_start_ms;
static uint8_t pairing_failure_count;
static unsigned int reject_pairing_time_start_ms;
static uint64_t peer_public_address;
static int advertisement_mode;
static nearby_platform_AccountKeyInfo account_key_info;
static nearby_platform_AccountKeyInfo pending_account_key_info;
static uint64_t gatt_peer_address;
static uint32_t address_rotation_timestamp;
static void* address_rotation_task = NULL;
#if NEARBY_FP_ENABLE_ADDITIONAL_DATA
static uint8_t additional_data_id;
#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */
#ifdef NEARBY_FP_ENABLE_SASS
static const uint16_t kSassVersion = 0x101;
static uint8_t sass_custom_data = 0;
#endif /* NEARBY_FP_ENABLE_SASS */

#define RETURN_IF_ERROR(X)                        \
  do {                                            \
    nearby_platform_status status = X;            \
    if (kNearbyStatusOK != status) return status; \
  } while (0)

#define BIT(b) (1 << b)
#define ISSET(v, b) (v & BIT(b))

#if NEARBY_FP_MESSAGE_STREAM
// Callback triggered when a complete message is received over message stream
static void OnMessageReceived(uint64_t peer_address,
                              nearby_message_stream_Message* message);
static void SendBleAddressUpdatedToAll();

typedef struct {
  nearby_message_stream_State state;
  // Random session nonce generated by the provider. It is used to authenticate
  // the seeker and prevent replay attacks
  uint8_t session_nonce[SESSION_NONCE_SIZE];
  uint8_t in_use_account_key[ACCOUNT_KEY_SIZE_BYTES];
  uint8_t buffer[MAX_MESSAGE_STREAM_PAYLOAD_SIZE +
                 sizeof(nearby_message_stream_Metadata)];
  uint8_t capabilities;
  uint8_t platform_type;
  uint8_t platform_build;
} rfcomm_input;

static void InitRfcommInput(uint64_t peer_address, rfcomm_input* input) {
  memset(input, 0, sizeof(*input));
  input->state.peer_address = peer_address;
  input->state.on_message_received = OnMessageReceived;
  for (int i = 0; i < SESSION_NONCE_SIZE; i++) {
    input->session_nonce[i] = nearby_platform_Rand();
  }
  input->state.length = sizeof(input->buffer);
  input->state.buffer = input->buffer;
  // defaults to: companion app installed, silence mode supported
  input->capabilities = BIT(MESSAGE_CODE_CAPABILITIES_COMPANION_APP_INSTALLED) |
                        BIT(MESSAGE_CODE_CAPABILITIES_SILENCE_MODE_SUPPORTED);
  nearby_message_stream_Init(&input->state);
}

static rfcomm_input rfcomm_inputs[NEARBY_MAX_RFCOMM_CONNECTIONS];
#ifdef NEARBY_FP_ENABLE_SASS
static void UpdateAndNotifySass(const rfcomm_input* seeker);
#endif /* NEARBY_FP_ENABLE_SASS */

#endif /* NEARBY_FP_MESSAGE_STREAM */

#if NEARBY_FP_RETROACTIVE_PAIRING
typedef struct {
  uint64_t peer_public_address;
  uint64_t peer_le_address;
  unsigned int retroactive_pairing_time_start_ms;
} retroactive_pairing_peer;

static retroactive_pairing_peer
    retroactive_pairing_list[NEARBY_MAX_RETROACTIVE_PAIRING];

static bool AddRetroactivePairingPeer(uint64_t peer_address) {
  for (int i = 0; i < NEARBY_MAX_RETROACTIVE_PAIRING; i++) {
    retroactive_pairing_peer* peer = &retroactive_pairing_list[i];

    if (nearby_platform_GetCurrentTimeMs() -
            peer->retroactive_pairing_time_start_ms >
        RETRO_PAIRING_REQUEST_TIME_MS) {
      peer->peer_public_address = INVALID_PEER_ADDRESS;
      peer->peer_le_address = INVALID_PEER_ADDRESS;
    }

    if (peer->peer_public_address == peer_address) {
      return false;
    }
  }

  for (int i = 0; i < NEARBY_MAX_RETROACTIVE_PAIRING; i++) {
    retroactive_pairing_peer* peer = &retroactive_pairing_list[i];
    if (peer->peer_public_address == INVALID_PEER_ADDRESS) {
      NEARBY_TRACE(INFO, "timer set for retroactive pairing");
      peer->peer_public_address = peer_address;
      peer->retroactive_pairing_time_start_ms =
          nearby_platform_GetCurrentTimeMs();
      return true;
    }
  }
  return false;
}

static bool SetRetroactivePairingPeerLe(uint64_t peer_public_address,
                                        uint64_t peer_le_address) {
  for (int i = 0; i < NEARBY_MAX_RETROACTIVE_PAIRING; i++) {
    retroactive_pairing_peer* peer = &retroactive_pairing_list[i];
    if (peer->peer_public_address == peer_public_address) {
      peer->peer_le_address = peer_le_address;
      return true;
    }
  }
  return false;
}

static bool RetroactivePairingPeerPending(uint64_t peer_address) {
  for (int i = 0; i < NEARBY_MAX_RETROACTIVE_PAIRING; i++) {
    retroactive_pairing_peer* peer = &retroactive_pairing_list[i];
    if (peer->peer_public_address == peer_address ||
        peer->peer_le_address == peer_address) {
      return true;
    }
  }
  return false;
}

static bool RetroactivePairingPeerTimeout(uint64_t peer_address) {
  for (int i = 0; i < NEARBY_MAX_RETROACTIVE_PAIRING; i++) {
    retroactive_pairing_peer* peer = &retroactive_pairing_list[i];
    if ((peer->peer_public_address == peer_address ||
         peer->peer_le_address == peer_address) &&
        (nearby_platform_GetCurrentTimeMs() -
             peer->retroactive_pairing_time_start_ms >
         RETRO_PAIRING_REQUEST_TIME_MS)) {
      return true;
    }
  }
  return false;
}

static void RemoveRetroactivePairingPeer(uint64_t peer_address) {
  for (int i = 0; i < NEARBY_MAX_RETROACTIVE_PAIRING; i++) {
    retroactive_pairing_peer* peer = &retroactive_pairing_list[i];
    if (peer->peer_public_address == peer_address ||
        peer->peer_le_address == peer_address) {
      peer->peer_public_address = INVALID_PEER_ADDRESS;
      peer->peer_le_address = INVALID_PEER_ADDRESS;
    }
  }
}
#endif /* NEARBY_FP_RETROACTIVE_PAIRING */

static bool BtAddressMatch(uint8_t* a, uint8_t* b) {
  return 0 == memcmp(a, b, BT_ADDRESS_LENGTH);
}

static bool ShowPairingIndicator() {
  return advertisement_mode & NEARBY_FP_ADVERTISEMENT_PAIRING_UI_INDICATOR;
}

#ifdef NEARBY_FP_ENABLE_SASS

static bool IsSassSeeker(const rfcomm_input* input) {
  return input->state.peer_address != INVALID_PEER_ADDRESS &&
         input->in_use_account_key[0] == 4;
}

static bool IsDeviceSass(uint64_t peer_address) {
  for (int i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    rfcomm_input* input = &rfcomm_inputs[i];
    if (input->state.peer_address == peer_address) {
      return IsSassSeeker(input);
    }
  }
  return false;
}

static uint8_t* FindInUseKey() {
  for (int i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    rfcomm_input* input = &rfcomm_inputs[i];
    if (IsSassSeeker(input)) {
      return input->in_use_account_key;
    }
  }
  return NULL;
}

static uint8_t GetCustomData() { return sass_custom_data; }
static void SetCustomData(uint8_t data) { sass_custom_data = data; }

#endif /* NEARBY_FP_ENABLE_SASS */

static bool IncludeSass() {
#ifdef NEARBY_FP_ENABLE_SASS
  return advertisement_mode & NEARBY_FP_ADVERTISEMENT_SASS;
#else
  return false;
#endif /* NEARBY_FP_ENABLE_SASS */
}

static uint8_t* SelectInUseKey() {
#ifdef NEARBY_FP_ENABLE_SASS
  return IncludeSass() ? FindInUseKey() : NULL;
#else
  return NULL;
#endif /* NEARBY_FP_ENABLE_SASS */
}

static bool IsInPairingMode() {
  return nearby_platform_IsInPairingMode() ||
         (pairing_state != kPairingStateIdle &&
          pairing_state != kPairingStateWaitingForAccountKeyWrite &&
          pairing_state != kPairingStateWaitingForAdditionalData);
}

#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
static bool ShowBatteryIndicator() {
  return advertisement_mode & NEARBY_FP_ADVERTISEMENT_BATTERY_UI_INDICATOR;
}

static bool IncludeBatteryInfo() {
  return advertisement_mode & NEARBY_FP_ADVERTISEMENT_INCLUDE_BATTERY_INFO;
}

// Gets battery info. Returns the input BatteryInfo or NULL on error.
static nearby_platform_BatteryInfo* PrepareBatteryInfo(
    nearby_platform_BatteryInfo* battery_info) {
  nearby_platform_status status;

  battery_info->is_charging = false;
  battery_info->right_bud_battery_level = battery_info->left_bud_battery_level =
      battery_info->charging_case_battery_level = INVALID_BATTERY_LEVEL;
  battery_info->remaining_time_minutes = 0;
  status = nearby_platform_GetBatteryInfo(battery_info);
  if (status != kNearbyStatusOK) {
    NEARBY_TRACE(ERROR, "GetBatteryInfo() failed with error %d", status);
    return NULL;
  }
  return battery_info;
}

#if NEARBY_FP_MESSAGE_STREAM
static nearby_platform_status SendBatteryInfoMessage(uint64_t peer_address) {
  uint8_t levels[BATTERY_LEVELS_SIZE];
  nearby_message_stream_Message message = {
      .message_group = MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
      .message_code = MESSAGE_CODE_BATTERY_UPDATED,
      .length = sizeof(levels),
      .data = levels};
  nearby_platform_BatteryInfo battery_info;
  if (!PrepareBatteryInfo(&battery_info)) return kNearbyStatusOK;

  SerializeBatteryInfo(message.data, &battery_info);
  return nearby_message_stream_Send(peer_address, &message);
}

static nearby_platform_status SendBatteryTimeMessage(uint64_t peer_address) {
  uint8_t time[BATTERY_TIME_SIZE];
  nearby_message_stream_Message message = {
      .message_group = MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
      .message_code = MESSAGE_CODE_REMAINING_BATTERY_TIME,
      .length = 1,
      .data = time};
  nearby_platform_BatteryInfo battery_info;
  if (!PrepareBatteryInfo(&battery_info)) return kNearbyStatusOK;

  if (battery_info.remaining_time_minutes > 255) {
    message.length = sizeof(int16_t);
    message.data[0] = battery_info.remaining_time_minutes >> 8;
    message.data[1] = battery_info.remaining_time_minutes & 0xff;
  } else {
    message.data[0] = battery_info.remaining_time_minutes;
  }
  return nearby_message_stream_Send(peer_address, &message);
}
#endif /* NEARBY_FP_MESSAGE_STREAM */
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

static void AccountKeyRejected() {
  if (++pairing_failure_count == MAX_PAIRING_FAILURE_COUNT) {
    reject_pairing_time_start_ms = nearby_platform_GetCurrentTimeMs();
  }
}

static void DiscardAccountKey() {
  memset(&account_key_info, 0, sizeof(account_key_info));
}

static bool ShouldTimeout(unsigned int timeout_ms) {
  return nearby_platform_GetCurrentTimeMs() - timeout_start_ms > timeout_ms;
}

static bool HasPendingAccountKey() {
  return pending_account_key_info.account_key[0] ==
         ACCOUNT_KEY_WRITE_MESSAGE_TYPE;
}

static void DiscardPendingAccountKey() {
  memset(&pending_account_key_info, 0, sizeof(pending_account_key_info));
}

static void RotateBleAddress() {
  address_rotation_timestamp = nearby_platform_GetCurrentTimeMs();
  nearby_platform_SetAdvertisement(NULL, 0, kDisabled);
#ifdef NEARBY_FP_HAVE_BLE_ADDRESS_ROTATION
  uint64_t address = nearby_platform_RotateBleAddress();
  NEARBY_TRACE(INFO, "Rotated BLE address to: %s",
               nearby_utils_MacToString(address));
#else
  unsigned i;
  uint64_t address = 0;
  for (i = 0; i < BT_ADDRESS_LENGTH; i++) {
    address = (address << 8) ^ nearby_platform_Rand();
  }
  address |= (uint64_t)1 << 46;
  address &= ~((uint64_t)1 << 47);
  NEARBY_TRACE(WARNING, "Rotate address to: %s",
               nearby_utils_MacToString(address));
  nearby_platform_SetBleAddress(address);
#if NEARBY_FP_MESSAGE_STREAM
  SendBleAddressUpdatedToAll();
#endif
#endif /* NEARBY_FP_HAVE_BLE_ADDRESS_ROTATION */
}

static nearby_platform_status SendKeyBasedPairingResponse(
    uint64_t peer_address, bool extended_response) {
  nearby_platform_status status;
  uint8_t raw[AES_MESSAGE_SIZE_BYTES], encrypted[AES_MESSAGE_SIZE_BYTES];
  nearby_fp_CreateRawKeybasedPairingResponse(raw, extended_response);
  status = nearby_platform_Aes128Encrypt(raw, encrypted,
                                         account_key_info.account_key);
  if (status != kNearbyStatusOK) {
    NEARBY_TRACE(ERROR, "Failed to encrypt key-based pairing response");
    return status;
  }
  status = nearby_platform_GattNotify(peer_address, kKeyBasedPairing, encrypted,
                                      sizeof(encrypted));
  if (status != kNearbyStatusOK) {
    NEARBY_TRACE(ERROR, "Failed to notify on key-based pairing response");
  }
  return status;
}

#if NEARBY_FP_ENABLE_ADDITIONAL_DATA
static nearby_platform_status NotifyPersonalizedName(uint64_t peer_address) {
  NEARBY_TRACE(VERBOSE, "NotifyPersonalizedName");
  uint8_t data[ADDITIONAL_DATA_HEADER_SIZE + PERSONALIZED_NAME_MAX_SIZE];
  size_t length = PERSONALIZED_NAME_MAX_SIZE;
  nearby_platform_status status;
  status = nearby_platform_LoadValue(
      kStoredKeyPersonalizedName, data + ADDITIONAL_DATA_HEADER_SIZE, &length);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(WARNING, "Failed to load personalized name, status: %d",
                 status);
    return status;
  }
  if (!length) {
    NEARBY_TRACE(WARNING, "Empty personalized name");
  }

  length += ADDITIONAL_DATA_HEADER_SIZE;
  status = nearby_fp_EncodeAdditionalData(data, length,
                                          account_key_info.account_key);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(ERROR, "Failed to encrypt additional data, status: %d",
                 status);
    return status;
  }
  status =
      nearby_platform_GattNotify(peer_address, kAdditionalData, data, length);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(ERROR, "Failed to notify on additional data, status: %d",
                 status);
    NEARBY_TRACE(ERROR, "Additional data: %s",
                 nearby_utils_ArrayToString(data, length));
  }
  return kNearbyStatusOK;
}

static nearby_platform_status SaveAdditionalData(const uint8_t* data,
                                                 size_t length) {
  if (additional_data_id == PERSONALIZED_NAME_DATA_ID) {
    char name[PERSONALIZED_NAME_MAX_SIZE * sizeof(uint8_t) / sizeof(char) + 1];
    memcpy(name, data, length);
    name[length * sizeof(uint8_t) / sizeof(char)] = '\0';
    NEARBY_TRACE(INFO, "Saving personalized name: %s", name);
    nearby_platform_SetDeviceName(name);
    return nearby_platform_SaveValue(kStoredKeyPersonalizedName, data, length);
  }
  NEARBY_TRACE(WARNING, "Unsupported data id 0x%02x", additional_data_id);
  return kNearbyStatusUnsupported;
}

static nearby_platform_status OnAdditionalDataWrite(uint64_t peer_address,
                                                    const uint8_t* request,
                                                    size_t length) {
  NEARBY_TRACE(VERBOSE, "OnAdditionalDataWrite");
  if (pairing_state != kPairingStateWaitingForAdditionalData) {
    NEARBY_TRACE(WARNING,
                 "Not expecting a write to Additional Data. Current state: %d",
                 pairing_state);
    return kNearbyStatusError;
  }
  nearby_platform_status status = nearby_fp_DecodeAdditionalData(
      (uint8_t*)request, length, account_key_info.account_key);
  DiscardAccountKey();
  if (kNearbyStatusOK == status) {
    status = SaveAdditionalData(request + ADDITIONAL_DATA_HEADER_SIZE,
                                length - ADDITIONAL_DATA_HEADER_SIZE);
  }
  pairing_state = kPairingStateIdle;
  return status;
}
#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */

static nearby_platform_status HandleKeyBasedPairingRequest(
    uint64_t peer_address, uint8_t request[ENCRYPTED_REQUEST_LENGTH]) {
  NEARBY_TRACE(VERBOSE, "HandleKeyBasedPairingRequest");
  int flags;
  flags = request[1];
#if NEARBY_FP_ENABLE_ADDITIONAL_DATA
  if (flags & KBPR_NOTIFY_EXISTING_NAME_MASK) {
    NotifyPersonalizedName(peer_address);
  }
#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */

  if (flags & KBPR_RETROACTIVELY_WRITE_ACCOUNT_KEY_MASK) {
    if (flags & KBPR_INITIATE_PAIRING_MASK) {
      NEARBY_TRACE(WARNING,
                   "received flag Initiate pairing in retroactive pairing");
    }

#if NEARBY_FP_RETROACTIVE_PAIRING
    uint64_t peer_public_address =
        nearby_utils_GetBigEndian48(request + KBPR_SEEKER_ADDRESS_OFFSET);

    if (RetroactivePairingPeerPending(peer_public_address) == false) {
      NEARBY_TRACE(
          ERROR, "Ignoring retroactive pairing request from unexpected client");
      RemoveRetroactivePairingPeer(peer_public_address);
      return kNearbyStatusError;
    }

    if (SetRetroactivePairingPeerLe(peer_public_address, peer_address) ==
        false) {
      NEARBY_TRACE(ERROR, "Cannot set le address for retroactive pairing");
      RemoveRetroactivePairingPeer(peer_public_address);
      return kNearbyStatusError;
    }

    return kNearbyStatusOK;
#else
    return kNearbyStatusUnsupported;
#endif /* NEARBY_FP_RETROACTIVE_PAIRING */
  }

  nearby_platform_SetFastPairCapabilities();
  if (flags & KBPR_INITIATE_PAIRING_MASK) {
    peer_public_address =
        nearby_utils_GetBigEndian48(request + KBPR_SEEKER_ADDRESS_OFFSET);
    NEARBY_TRACE(INFO, "Send pairing request to %s",
                 nearby_utils_MacToString(peer_public_address));
    nearby_platform_SendPairingRequest(peer_public_address);
    pairing_state = kPairingStateWaitingForPasskey;
  } else {
    pairing_state = kPairingStateWaitingForPairingRequest;
    timeout_start_ms = nearby_platform_GetCurrentTimeMs();
  }
  return kNearbyStatusOK;
}

static nearby_platform_status HandleActionRequest(
    uint64_t peer_address, uint8_t request[ENCRYPTED_REQUEST_LENGTH]) {
  NEARBY_TRACE(VERBOSE, "HandleActionRequest");
  int flags;
  flags = request[1];
  if (flags & ACTION_REQUEST_DEVICE_ACTION_MASK) {
    NEARBY_TRACE(VERBOSE, "Device action not implemented");
    return kNearbyStatusUnimplemented;
  }
#if NEARBY_FP_ENABLE_ADDITIONAL_DATA
  if (flags & ACTION_REQUEST_WILL_WRITE_DATA_CHARACTERISTIC_MASK) {
    pairing_state = kPairingStateWaitingForAdditionalData;
    additional_data_id = request[10];
  }
  return kNearbyStatusOK;
#else
  return kNearbyStatusUnsupported;
#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */
}

static nearby_platform_status OnWriteKeyBasedPairing(uint64_t peer_address,
                                                     const uint8_t* request,
                                                     size_t length) {
  nearby_platform_status status;
  uint8_t decrypted_request[ENCRYPTED_REQUEST_LENGTH];
  uint8_t ble_address[BT_ADDRESS_LENGTH];
  uint8_t public_address[BT_ADDRESS_LENGTH];

  NEARBY_TRACE(VERBOSE, "OnWriteKeyBasedPairing");
  if (pairing_failure_count >= MAX_PAIRING_FAILURE_COUNT) {
    unsigned int current_time = nearby_platform_GetCurrentTimeMs();
    if (current_time - reject_pairing_time_start_ms <
        REJECT_PAIRING_TIMEOUT_MS) {
      NEARBY_TRACE(INFO, "Too many failed pairing attempts");
      return kNearbyStatusOK;
    } else {
      NEARBY_TRACE(INFO, "Timeout expired. Allow pairing attempts");
      pairing_failure_count = 0;
    }
  }
  nearby_utils_CopyBigEndian(ble_address, nearby_platform_GetBleAddress(),
                             BT_ADDRESS_LENGTH);
  nearby_utils_CopyBigEndian(public_address, nearby_platform_GetPublicAddress(),
                             BT_ADDRESS_LENGTH);
  // When the device is nondiscoverable, accept a saved account key.
  // or accept a new account key for retroactive pairing
  // When the device is discoverable, we can accept a new account key too.
  if (length == ENCRYPTED_REQUEST_LENGTH + PUBLIC_KEY_LENGTH) {
    uint8_t key[ACCOUNT_KEY_SIZE_BYTES];
    uint8_t remote_public_key[PUBLIC_KEY_LENGTH];
    memcpy(remote_public_key, request + ENCRYPTED_REQUEST_LENGTH,
           PUBLIC_KEY_LENGTH);
    status = nearby_fp_CreateSharedSecret(remote_public_key, key);
    if (status != kNearbyStatusOK) {
      NEARBY_TRACE(ERROR, "Failed to create shared key, error: %d", status);
      NEARBY_TRACE(
          ERROR, "Remote public key: %s",
          nearby_utils_ArrayToString(remote_public_key, PUBLIC_KEY_LENGTH));
      return status;
    }
    status = nearby_platform_Aes128Decrypt(request, decrypted_request, key);
    if (status != kNearbyStatusOK) {
      NEARBY_TRACE(ERROR, "Failed to decrypt request, error: %d", status);
      return status;
    }
    if (!BtAddressMatch(ble_address,
                        decrypted_request + REQUEST_BT_ADDRESS_OFFSET) &&
        !BtAddressMatch(public_address,
                        decrypted_request + REQUEST_BT_ADDRESS_OFFSET)) {
      NEARBY_TRACE(INFO, "Invalid incoming BT address %s",
                   nearby_utils_ArrayToString(decrypted_request, 6));
      AccountKeyRejected();
      return kNearbyStatusOK;
    }
    memcpy(account_key_info.account_key, key, ACCOUNT_KEY_SIZE_BYTES);
#ifdef NEARBY_FP_ENABLE_SASS
    account_key_info.peer_address = peer_address;
#endif /* NEARBY_FP_ENABLE_SASS */
  } else if (length == ENCRYPTED_REQUEST_LENGTH) {
    // try each key in the persisted Account Key List
    int num_keys = nearby_fp_GetAccountKeyCount();
    int i;
    for (i = 0; i < num_keys; i++) {
      const nearby_platform_AccountKeyInfo* key = nearby_fp_GetAccountKey(i);
      status = nearby_platform_Aes128Decrypt(request, decrypted_request,
                                             key->account_key);
      if (status != kNearbyStatusOK) {
        NEARBY_TRACE(ERROR, "Failed to decrypt request, error: %d", status);
        return status;
      }
      if (BtAddressMatch(ble_address,
                         decrypted_request + REQUEST_BT_ADDRESS_OFFSET) ||
          BtAddressMatch(public_address,
                         decrypted_request + REQUEST_BT_ADDRESS_OFFSET)) {
        NEARBY_TRACE(VERBOSE, "Matched key number: %d", i);
        nearby_fp_CopyAccountKey(&account_key_info, i);
#ifdef NEARBY_FP_ENABLE_SASS
        // The existing account key could be for a different peer, so let's
        // add it again with the current peer address
        account_key_info.peer_address = peer_address;
#endif /* NEARBY_FP_ENABLE_SASS */
        nearby_fp_AddAccountKey(&account_key_info);
        nearby_fp_SaveAccountKeys();
        break;
      }
    }
    if (i == num_keys) {
      NEARBY_TRACE(VERBOSE, "No key matched");
      AccountKeyRejected();
      return kNearbyStatusOK;
    }
  } else {
    NEARBY_TRACE(WARNING,
                 "Unexpected key based pairing request length %d Request: %s",
                 length, nearby_utils_ArrayToString(request, length));
    return kNearbyStatusError;
  }
  pairing_failure_count = 0;
  gatt_peer_address = peer_address;

  DiscardPendingAccountKey();

  bool extended_response =
      decrypted_request[0] == KEY_BASED_PAIRING_REQUEST_FLAG &&
      decrypted_request[1] & KBPR_SEEKER_SUPPORTS_BLE_DEVICES_MASK;
  status = SendKeyBasedPairingResponse(peer_address, extended_response);
  if (status != kNearbyStatusOK) return status;

  // TODO(jsobczak): Note that at the end of the packet there is a salt
  // attached. When possible, these salts should be tracked, and if the
  // Provider receives a request containing an already used salt, the request
  // should be ignored to prevent replay attacks.
  if (decrypted_request[0] == KEY_BASED_PAIRING_REQUEST_FLAG) {
    return HandleKeyBasedPairingRequest(peer_address, decrypted_request);
  } else if (decrypted_request[0] == ACTION_REQUEST_FLAG) {
    return HandleActionRequest(peer_address, decrypted_request);
  }
  return kNearbyStatusOK;
}

static nearby_platform_status NotifyProviderPasskey(uint64_t peer_address) {
  uint8_t raw_passkey_block[AES_MESSAGE_SIZE_BYTES];
  uint8_t encrypted[AES_MESSAGE_SIZE_BYTES];
  uint32_t provider_passkey;
  nearby_platform_status status;
  provider_passkey = nearby_platfrom_GetPairingPassKey();

  raw_passkey_block[0] = PROVIDER_PASSKEY_MESSAGE_TYPE;
  nearby_utils_CopyBigEndian(raw_passkey_block + 1, provider_passkey, 3);
  int i;
  for (i = 4; i < AES_MESSAGE_SIZE_BYTES; i++) {
    raw_passkey_block[i] = nearby_platform_Rand();
  }
  status = nearby_platform_Aes128Encrypt(raw_passkey_block, encrypted,
                                         account_key_info.account_key);
  if (status != kNearbyStatusOK) {
    NEARBY_TRACE(ERROR, "Failed to encrypt passkey block");
    return status;
  }
  status = nearby_platform_GattNotify(peer_address, kPasskey, encrypted,
                                      sizeof(encrypted));
  if (status != kNearbyStatusOK) {
    NEARBY_TRACE(ERROR,
                 "Failed to notify on passkey characteristic, "
                 "Peer address: %s Encrypted: %s",
                 nearby_utils_MacToString(peer_address),
                 nearby_utils_ArrayToString(encrypted, sizeof(encrypted)));
  }
  return status;
}

static nearby_platform_status OnPasskeyWrite(uint64_t peer_address,
                                             const uint8_t* request,
                                             size_t length) {
  NEARBY_TRACE(VERBOSE, "OnPasskeyWrite");
  uint8_t raw_passkey_block[AES_MESSAGE_SIZE_BYTES];
  nearby_platform_status status;
  uint32_t seeker_passkey;
  if (pairing_state != kPairingStateWaitingForPasskey) {
    NEARBY_TRACE(INFO, "Not expecting a passkey write. Current state: %d",
                 pairing_state);
    return kNearbyStatusError;
  }
  if (gatt_peer_address != peer_address) {
    NEARBY_TRACE(INFO,
                 "Ignoring passkey write from unexpected client, expected GATT "
                 "peer address: %s",
                 nearby_utils_MacToString(gatt_peer_address));
    NEARBY_TRACE(INFO, "Peer address: %s",
                 nearby_utils_MacToString(peer_address));
    return kNearbyStatusError;
  }
  if (ShouldTimeout(PASSKEY_MAX_WAIT_TIME_MS)) {
    NEARBY_TRACE(INFO, "Not expecting a passkey write");
    return kNearbyStatusTimeout;
  }
  if (length != AES_MESSAGE_SIZE_BYTES) {
    NEARBY_TRACE(INFO, "Passkey: expected %d bytes, got %d request: %s",
                 AES_MESSAGE_SIZE_BYTES, length,
                 nearby_utils_ArrayToString(request, length));
    return kNearbyStatusError;
  }
  status = nearby_platform_Aes128Decrypt(request, raw_passkey_block,
                                         account_key_info.account_key);
  if (status != kNearbyStatusOK) {
    NEARBY_TRACE(WARNING, "Failed to decrypt passkey block");
    DiscardAccountKey();
    return status;
  }
  if (raw_passkey_block[0] != SEEKER_PASSKEY_MESSAGE_TYPE) {
    NEARBY_TRACE(WARNING, "Unexpected passkey message type 0x%x",
                 raw_passkey_block[0]);
    return kNearbyStatusError;
  }
  pairing_state = kPairingStateWaitingForPairingResult;
  NotifyProviderPasskey(peer_address);
  seeker_passkey = nearby_utils_GetBigEndian24(raw_passkey_block + 1);
  nearby_platform_SetRemotePasskey(seeker_passkey);
  return kNearbyStatusOK;
}

static nearby_platform_status SetNonDiscoverableAdvertisement() {
  uint8_t advertisement[NON_DISCOVERABLE_ADV_SIZE_BYTES];
  const uint8_t* in_use_key = SelectInUseKey();
  size_t length;
#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
  nearby_platform_BatteryInfo battery_info;
  length = nearby_fp_CreateNondiscoverableAdvertisementWithBattery(
      advertisement, sizeof(advertisement), ShowPairingIndicator(),
      ShowBatteryIndicator(),
      IncludeBatteryInfo() ? PrepareBatteryInfo(&battery_info) : NULL);
#else
  length = nearby_fp_CreateNondiscoverableAdvertisement(
      advertisement, sizeof(advertisement), ShowPairingIndicator());
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */
#ifdef NEARBY_FP_ENABLE_SASS
  if (IncludeSass() && (nearby_fp_GetAccountKeyCount() > 0)) {
    uint8_t device_bitmap[MAX_DEVICE_BITMAP_SIZE];
    size_t device_bitmap_length = sizeof(device_bitmap);
    // Use either the in-use key or the most recently used account key
    const uint8_t* key =
        in_use_key ? in_use_key : nearby_fp_GetAccountKey(0)->account_key;
    NEARBY_ASSERT(key != NULL);
    nearby_platform_GetConnectionBitmap(device_bitmap, &device_bitmap_length);
    size_t sass_length = nearby_fp_GenerateSassAdvertisement(
        advertisement + length + RRF_HEADER_SIZE,
        sizeof(advertisement) - length - RRF_HEADER_SIZE,
        nearby_fp_GetSassConnectionState(), GetCustomData(), device_bitmap,
        device_bitmap_length);
    RETURN_IF_ERROR(nearby_fp_EncryptRandomResolvableField(
        advertisement + length, RRF_HEADER_SIZE + sass_length, key,
        nearby_fp_FindLtv(advertisement, SALT_FIELD_TYPE)));
    length += RRF_HEADER_SIZE + sass_length;
    // Update FP advertisement length
    advertisement[0] = length - 1;
  }
#endif /* NEARBY_FP_ENABLE_SASS */
  nearby_fp_SetBloomFilter(advertisement, IncludeSass(), in_use_key);

  length += nearby_fp_AppendTxPower(advertisement + length,
                                    sizeof(advertisement) - length,
                                    nearby_platform_GetTxLevel());
  return nearby_platform_SetAdvertisement(advertisement, length,
                                          kNoLargerThan250ms);
}

// Steps executed after successful pairing with a seeker and receiving the
// account key.
static void RunPostPairingSteps(
    uint64_t peer_address, const nearby_platform_AccountKeyInfo* account_key) {
  nearby_fp_AddAccountKey(account_key);
  nearby_fp_SaveAccountKeys();
  DiscardPendingAccountKey();
#if NEARBY_FP_RETROACTIVE_PAIRING
  if (RetroactivePairingPeerPending(peer_address)) {
    RemoveRetroactivePairingPeer(peer_address);

    if (RetroactivePairingPeerPending(peer_address)) {
      NEARBY_TRACE(WARNING, "peer is still pending %s",
                   nearby_utils_MacToString(peer_address));
    }
    if (pairing_state != kPairingStateIdle) {
      NEARBY_TRACE(WARNING,
                   "Another fp pairing process has launched, do not change "
                   "pairing state");
      return;
    }
  }
#endif /* NEARBY_FP_RETROACTIVE_PAIRING */

  pairing_state = kPairingStateIdle;
#if NEARBY_FP_ENABLE_ADDITIONAL_DATA
  pairing_state = kPairingStateWaitingForAdditionalData;
  additional_data_id = PERSONALIZED_NAME_DATA_ID;
#endif
  if (advertisement_mode & NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE) {
    NEARBY_TRACE(INFO, "Account key added, update advertisement");
    nearby_platform_SetAdvertisement(NULL, 0, kDisabled);
    SetNonDiscoverableAdvertisement();
  }
}
static nearby_platform_status OnAccountKeyWrite(uint64_t peer_address,
                                                const uint8_t* request,
                                                size_t length) {
  NEARBY_TRACE(VERBOSE, "OnAccountKeyWrite");
  nearby_platform_AccountKeyInfo key_info;
  uint8_t* decrypted_request = key_info.account_key;
  nearby_platform_status status;
  bool wait_until_paired = false;

#if NEARBY_FP_RETROACTIVE_PAIRING
  if (RetroactivePairingPeerPending(peer_address)) {
    if (RetroactivePairingPeerTimeout(peer_address)) {
      NEARBY_TRACE(ERROR,
                   "Not expecting an retroactive pairing request. Timeout. "
                   "Peer address: %s",
                   nearby_utils_MacToString(peer_address));
      RemoveRetroactivePairingPeer(peer_address);
      return kNearbyStatusTimeout;
    }
  } else {
#endif /* NEARBY_FP_RETROACTIVE_PAIRING */
    if (pairing_state == kPairingStateWaitingForPairingRequest ||
        pairing_state == kPairingStateWaitingForPairingResult ||
        pairing_state == kPairingStateWaitingForPasskey) {
      NEARBY_TRACE(VERBOSE, "Account key write before paired event");
      wait_until_paired = true;
    } else if (pairing_state != kPairingStateWaitingForAccountKeyWrite) {
      NEARBY_TRACE(INFO,
                   "Not expecting an account key write. Current state: %d",
                   pairing_state);
      return kNearbyStatusError;
    }
    if (gatt_peer_address != peer_address &&
        peer_public_address != peer_address) {
      NEARBY_TRACE(INFO, "Ignoring account key write from unexpected client");
      NEARBY_TRACE(ERROR, "Peer address: %s",
                   nearby_utils_MacToString(peer_address));
      NEARBY_TRACE(ERROR, "Expected BLE peer address: %s",
                   nearby_utils_MacToString(gatt_peer_address));
      NEARBY_TRACE(ERROR, "Expected BT peer address: %s",
                   nearby_utils_MacToString(peer_public_address));
      return kNearbyStatusError;
    }

    if (ShouldTimeout(ACCOUNT_KEY_WRITE_TIME_MS)) {
      NEARBY_TRACE(INFO, "Not expecting an account key write. Timeout");
      return kNearbyStatusTimeout;
    }
#if NEARBY_FP_RETROACTIVE_PAIRING
  }
#endif /* NEARBY_FP_RETROACTIVE_PAIRING */
  if (length != AES_MESSAGE_SIZE_BYTES) {
    NEARBY_TRACE(INFO, "Account key: expected %d bytes, got %d",
                 AES_MESSAGE_SIZE_BYTES, length);
    return kNearbyStatusError;
  }
  status = nearby_platform_Aes128Decrypt(request, decrypted_request,
                                         account_key_info.account_key);
  if (status != kNearbyStatusOK) {
    NEARBY_TRACE(WARNING, "Failed to decrypt account key block");
    return status;
  }
  if (decrypted_request[0] != ACCOUNT_KEY_WRITE_MESSAGE_TYPE) {
    NEARBY_TRACE(WARNING, "Unexpected account key message type 0x%x",
                 decrypted_request[0]);
    return kNearbyStatusError;
  }
#ifdef NEARBY_FP_ENABLE_SASS
  key_info.peer_address = peer_address;
#endif /* NEARBY_FP_ENABLE_SASS */
  if (wait_until_paired) {
    memcpy(pending_account_key_info.account_key, decrypted_request,
           ACCOUNT_KEY_SIZE_BYTES);
    nearby_platform_StartTimer(
        DiscardPendingAccountKey,
        WAIT_FOR_PAIRING_RESULT_AFTER_ACCOUNT_KEY_TIME_MS);
    return kNearbyStatusOK;
  }
  RunPostPairingSteps(peer_address, &key_info);
  return kNearbyStatusOK;
}

static nearby_platform_status OnGattWrite(
    uint64_t peer_address, nearby_fp_Characteristic characteristic,
    const uint8_t* request, size_t length) {
  switch (characteristic) {
    case kKeyBasedPairing:
      return OnWriteKeyBasedPairing(peer_address, request, length);
    case kPasskey:
      return OnPasskeyWrite(peer_address, request, length);
    case kAccountKey:
      return OnAccountKeyWrite(peer_address, request, length);
    case kAdditionalData:
#if NEARBY_FP_ENABLE_ADDITIONAL_DATA
      return OnAdditionalDataWrite(peer_address, request, length);
#else
      break;
#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */
    case kMessageStreamPsm:
    case kModelId:
    case kFirmwareRevision:
      break;
  }
  NEARBY_TRACE(ERROR,
               "Characteristic unsupported: Peer address: %s Characteristic: "
               "%d Request: %s",
               nearby_utils_MacToString(peer_address), characteristic,
               nearby_utils_ArrayToString(request, length));
  return kNearbyStatusUnsupported;
}

static int32_t GetMessageStreamPsm() {
#if NEARBY_FP_PREFER_LE_TRANSPORT
  return nearby_platform_GetMessageStreamPsm();
#else
  return -1;
#endif /* NEARBY_FP_PREFER_LE_TRANSPORT */
}

static nearby_platform_status OnMesssageStreamPsmRead(uint8_t* output,
                                                      size_t* length) {
  int32_t psm = GetMessageStreamPsm();
  if (*length < PSM_MESSAGE_LENGTH) {
    return kNearbyStatusError;
  }
  memset(output, 0, PSM_MESSAGE_LENGTH);
  *length = PSM_MESSAGE_LENGTH;
  if (psm >= 0 && psm <= 0xFFFF) {
    output[0] = 1;
    output[1] = psm >> 8;
    output[2] = psm;
  }
  return kNearbyStatusOK;
}

static nearby_platform_status OnGattRead(
    uint64_t peer_address, nearby_fp_Characteristic characteristic,
    uint8_t* output, size_t* length) {
  switch (characteristic) {
    case kModelId:
      return nearby_fp_GattReadModelId(output, length);
    case kMessageStreamPsm:
      return OnMesssageStreamPsmRead(output, length);
    case kKeyBasedPairing:
    case kPasskey:
    case kAccountKey:
    case kFirmwareRevision:
    case kAdditionalData:
      break;
  }
  return kNearbyStatusUnsupported;
}

static void OnPairingRequest(uint64_t peer_address) {
  NEARBY_TRACE(VERBOSE, "Pairing request from %s",
               nearby_utils_MacToString(peer_address));
  if (pairing_state == kPairingStateWaitingForPairingRequest) {
    if (ShouldTimeout(WAIT_FOR_PAIRING_REQUEST_TIME_MS)) {
      pairing_state = kPairingStateIdle;
    } else {
      pairing_state = kPairingStateWaitingForPasskey;
      peer_public_address = peer_address;
      timeout_start_ms = nearby_platform_GetCurrentTimeMs();
    }
  }
}

static void OnPaired(uint64_t peer_address) {
  NEARBY_TRACE(INFO, "Paired with %s", nearby_utils_MacToString(peer_address));
  if (peer_public_address == peer_address && HasPendingAccountKey()) {
    NEARBY_TRACE(INFO, "Saving pending account key");
    RunPostPairingSteps(peer_address, &pending_account_key_info);
    return;
  }
  peer_public_address = peer_address;
  if (pairing_state == kPairingStateWaitingForPairingResult) {
    pairing_state = kPairingStateWaitingForAccountKeyWrite;
    timeout_start_ms = nearby_platform_GetCurrentTimeMs();
  }
#if NEARBY_FP_RETROACTIVE_PAIRING
  else if (AddRetroactivePairingPeer(peer_address) == false) {
    NEARBY_TRACE(WARNING, "No more timer for retroactive pairing");
  }
#endif /* NEARBY_FP_RETROACTIVE_AIRING */
}

static void OnPairingFailed(uint64_t peer_address) {
  NEARBY_TRACE(ERROR, "Pairing failed with %s",
               nearby_utils_MacToString(peer_address));
  DiscardAccountKey();
  DiscardPendingAccountKey();
}

#if NEARBY_FP_MESSAGE_STREAM
// Finds and initializes an unused |rfcomm_input|. Returns NULL if not found.
static rfcomm_input* FindAvailableRfcommInput(uint64_t peer_address) {
  for (int i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    rfcomm_input* input = &rfcomm_inputs[i];
    if (input->state.peer_address == INVALID_PEER_ADDRESS ||
        input->state.peer_address == peer_address) {
      InitRfcommInput(peer_address, input);
      return input;
    }
  }
  return NULL;
}

// Gets |rfcomm_input| for |peer_address|. Returns NULL if not found.
static rfcomm_input* GetRfcommInput(uint64_t peer_address) {
  for (int i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    if (rfcomm_inputs[i].state.peer_address == peer_address) {
      return &rfcomm_inputs[i];
    }
  }
  return NULL;
}

nearby_platform_status nearby_fp_client_SetSilenceMode(uint64_t peer_address,
                                                       bool enable) {
  rfcomm_input* input = GetRfcommInput(peer_address);
  if (!input) {
    return kNearbyStatusError;
  }
  nearby_message_stream_Message message = {
      .message_group = MESSAGE_GROUP_BLUETOOTH,
      .message_code = MESSAGE_CODE_DISABLE_SILENCE_MODE,
      .length = 0};
  if (enable) message.message_code = MESSAGE_CODE_ENABLE_SILENCE_MODE;
  if (!input) {
    return kNearbyStatusError;
  }
  if (!ISSET(input->capabilities,
             MESSAGE_CODE_CAPABILITIES_SILENCE_MODE_SUPPORTED)) {
    return kNearbyStatusUnsupported;
  }
  return nearby_message_stream_Send(peer_address, &message);
}

nearby_platform_status nearby_fp_client_SignalLogBufferFull(
    uint64_t peer_address) {
  rfcomm_input* input = GetRfcommInput(peer_address);
  if (!input) {
    return kNearbyStatusError;
  }
  if (!ISSET(input->capabilities,
             MESSAGE_CODE_CAPABILITIES_COMPANION_APP_INSTALLED)) {
    return kNearbyStatusOK;
  }
  nearby_message_stream_Message message = {
      .message_group = MESSAGE_GROUP_COMPANION_APP_EVENT,
      .message_code = MESSAGE_CODE_LOG_BUFFER_FULL,
      .length = 0};
  return nearby_message_stream_Send(peer_address, &message);
}

static nearby_platform_status SendBleAddressUpdated(uint64_t peer_address) {
  uint8_t buffer[BT_ADDRESS_LENGTH];
  nearby_message_stream_Message message = {
      .message_group = MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
      .message_code = MESSAGE_CODE_BLE_ADDRESS_UPDATED,
      .length = BT_ADDRESS_LENGTH,
      .data = buffer};
  nearby_utils_CopyBigEndian(message.data, nearby_platform_GetBleAddress(),
                             BT_ADDRESS_LENGTH);
  return nearby_message_stream_Send(peer_address, &message);
}

static void SendBleAddressUpdatedToAll() {
  uint8_t buffer[BT_ADDRESS_LENGTH];
  nearby_message_stream_Message message = {
      .message_group = MESSAGE_GROUP_DEVICE_INFORMATION_EVENT,
      .message_code = MESSAGE_CODE_BLE_ADDRESS_UPDATED,
      .length = BT_ADDRESS_LENGTH,
      .data = buffer};
  nearby_utils_CopyBigEndian(message.data, nearby_platform_GetBleAddress(),
                             BT_ADDRESS_LENGTH);
  for (int i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    rfcomm_input* input = &rfcomm_inputs[i];
    if (input->state.peer_address != INVALID_PEER_ADDRESS) {
      nearby_message_stream_Send(input->state.peer_address, &message);
    }
  }
}

static void OnMessageStreamConnected(uint64_t peer_address) {
  nearby_platform_status status;
  uint8_t buffer[MAX_MESSAGE_STREAM_PAYLOAD_SIZE];
  nearby_message_stream_Message message;
  size_t length = sizeof(buffer);
  rfcomm_input* input;

  NEARBY_TRACE(VERBOSE, "OnMessageStreamConnected %s",
               nearby_utils_MacToString(peer_address));
  input = FindAvailableRfcommInput(peer_address);
  if (!input) {
    NEARBY_TRACE(WARNING, "Too many concurrent RFCOMM connections");
    return;
  }

  message.data = buffer;
  nearby_fp_GattReadModelId(message.data, &length);
  message.length = length;
  message.message_group = MESSAGE_GROUP_DEVICE_INFORMATION_EVENT;
  message.message_code = MESSAGE_CODE_MODEL_ID;

  status = nearby_message_stream_Send(peer_address, &message);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(ERROR, "Failed to send model id, status: %d Peer address: %s",
                 status, nearby_utils_MacToString(peer_address));
    return;
  }
  status = SendBleAddressUpdated(peer_address);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(ERROR,
                 "Failed to send ble address, status: %d Peer address: %s",
                 status, nearby_utils_MacToString(peer_address));
    return;
  }

  // Send session nonce
  message.message_code = MESSAGE_CODE_SESSION_NONCE;
  message.length = SESSION_NONCE_SIZE;
  message.data = input->session_nonce;
  status = nearby_message_stream_Send(peer_address, &message);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(ERROR, "Failed to send session nonce, status: %d", status);
    return;
  }

#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
  status = SendBatteryInfoMessage(peer_address);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(ERROR,
                 "Failed to send battery info, status: %d Peer address: %s",
                 status, nearby_utils_MacToString(peer_address));
    return;
  }
  status = SendBatteryTimeMessage(peer_address);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(ERROR,
                 "Failed to send battery info, status: %d Peer address: %s",
                 status, nearby_utils_MacToString(peer_address));
    return;
  }
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

  if (client_callbacks != NULL && client_callbacks->on_event != NULL) {
    nearby_event_MessageStreamConnected payload = {.peer_address =
                                                       peer_address};
    nearby_event_Event event = {
        .event_type = kNearbyEventMessageStreamConnected,
        .payload = (uint8_t*)&payload};
    client_callbacks->on_event(&event);
  }
}

static void OnMessageStreamDisconnected(uint64_t peer_address) {
  rfcomm_input* input;

  NEARBY_TRACE(VERBOSE, "OnMessageStreamDisconnected %s",
               nearby_utils_MacToString(peer_address));
  input = GetRfcommInput(peer_address);
  if (!input) {
    NEARBY_TRACE(WARNING, "Unexpected disconnection from %s",
                 nearby_utils_MacToString(peer_address));
    return;
  }
#ifdef NEARBY_FP_ENABLE_SASS
  bool update_advert = IncludeSass() && IsSassSeeker(input);
#endif /* NEARBY_FP_ENABLE_SASS */
  input->state.peer_address = INVALID_PEER_ADDRESS;

  if (client_callbacks != NULL && client_callbacks->on_event != NULL) {
    nearby_event_MessageStreamDisconnected payload = {.peer_address =
                                                          peer_address};
    nearby_event_Event event = {
        .event_type = kNearbyEventMessageStreamDisconnected,
        .payload = (uint8_t*)&payload};
    client_callbacks->on_event(&event);
  }
#if NEARBY_FP_RETROACTIVE_PAIRING
  RemoveRetroactivePairingPeer(peer_address);
#endif /*NEARBY_FP_RETROACTIVE_PAIRING */
#ifdef NEARBY_FP_ENABLE_SASS
  if (update_advert) {
    UpdateAndNotifySass(NULL);
  }
#endif /* NEARBY_FP_ENABLE_SASS */
}

static void OnMessageStreamReceived(uint64_t peer_address,
                                    const uint8_t* message, size_t length) {
  rfcomm_input* input = GetRfcommInput(peer_address);
  if (!input) {
    NEARBY_TRACE(WARNING, "Unexpected RFCOMM message from %s",
                 nearby_utils_MacToString(peer_address));
    return;
  }
  nearby_message_stream_Read(&input->state, message, length);
}

static nearby_platform_status VerifyMessageLength(
    uint64_t peer_address, const nearby_message_stream_Message* message,
    size_t expected_length) {
  if (message->length != expected_length) {
    NEARBY_TRACE(WARNING, "Invalid message(%d) length %d, expected %d",
                 message->message_code, message->length, expected_length);
    nearby_message_stream_SendNack(peer_address, message,
                                   /* fail reason= */ 0);
    return kNearbyStatusInvalidInput;
  }
  return kNearbyStatusOK;
}

// Sends either ACK or NACK response depending on |status|
static nearby_platform_status SendResponse(
    uint64_t peer_address, const nearby_message_stream_Message* message,
    nearby_platform_status status) {
  if (kNearbyStatusOK == status) {
    return nearby_message_stream_SendAck(peer_address, message);
  } else {
    // TODO(jsobczak): What should be the default fail reason?
    uint8_t fail_reason = status == kNearbyStatusRedundantAction
                              ? FAIL_REASON_REDUNDANT_DEVICE_ACTION
                              : 0;
    return nearby_message_stream_SendNack(peer_address, message, fail_reason);
  }
}

#ifdef NEARBY_FP_ENABLE_SASS
static uint8_t GetActiveDeviceFlag(uint64_t peer_address) {
  uint64_t active_address = nearby_platform_GetActiveAudioSource();
  if (IsDeviceSass(active_address) == false && active_address != 0x0) {
    return SEEKER_IS_NON_SASS;
  } else if (peer_address == active_address) {
    return SEEKER_IS_ACTIVE_DEVICE;
  } else {
    return SEEKER_IS_NOT_ACTIVE_DEVICE;
  }
}

static nearby_platform_status PrepareNotifyConnectionStatus(
    nearby_message_stream_Message* message, rfcomm_input* input) {
  size_t device_bitmap_length = MAX_DEVICE_BITMAP_SIZE;
  NEARBY_ASSERT(message->length >= MAX_NOTIFY_CONNECTION_STATUS_MESSAGE_SIZE);
  uint8_t iv[AES_MESSAGE_SIZE_BYTES];
  memcpy(iv, input->session_nonce, SESSION_NONCE_SIZE);
  for (int i = SESSION_NONCE_SIZE; i < AES_MESSAGE_SIZE_BYTES; i++) {
    iv[i] = nearby_platform_Rand();
  }
  size_t offset = 0;
  message->message_code = MESSAGE_CODE_SASS_NOTIFY_CONNECTION_STATUS;
  message->data[offset++] = GetActiveDeviceFlag(input->state.peer_address);
  message->data[offset++] = nearby_fp_GetSassConnectionState();
  message->data[offset++] = GetCustomData();

  nearby_platform_GetConnectionBitmap(message->data + offset,
                                      &device_bitmap_length);
  offset += device_bitmap_length;
  memcpy(message->data + offset, iv + SESSION_NONCE_SIZE, SESSION_NONCE_SIZE);
  // The first byte of |data| is not encrypted.
  RETURN_IF_ERROR(nearby_fp_AesEncryptIv(message->data + 1, offset - 1, iv,
                                         input->in_use_account_key));
  message->length = offset + SESSION_NONCE_SIZE;
  return kNearbyStatusOK;
}

static bool IsSameAccountKey(const rfcomm_input* a, const rfcomm_input* b) {
  return a == b || (a != NULL && b != NULL &&
                    memcmp(a->in_use_account_key, b->in_use_account_key,
                           ACCOUNT_KEY_SIZE_BYTES) == 0);
}

static bool UsesInUseAccountKey(const rfcomm_input* input,
                                const uint8_t* account_key) {
  return input != NULL && account_key != NULL &&
         memcmp(input->in_use_account_key, account_key,
                ACCOUNT_KEY_SIZE_BYTES) == 0;
}

static nearby_platform_status NotifySassConnectionStatus(
    const rfcomm_input* seeker) {
  const uint8_t* in_use_key = seeker == NULL ? SelectInUseKey() : NULL;
  for (int i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    rfcomm_input* input = &rfcomm_inputs[i];
    // Notify all seekers that are using the same key of the seeker
    // if seeker is NULL, notify all seekers that are using the selected in
    // use key
    if (IsSassSeeker(input) && (IsSameAccountKey(input, seeker) ||
                                UsesInUseAccountKey(input, in_use_key))) {
      uint8_t buffer[MAX_NOTIFY_CONNECTION_STATUS_MESSAGE_SIZE];
      nearby_message_stream_Message reply;
      reply.message_group = MESSAGE_GROUP_SASS;
      reply.data = buffer;
      reply.length = sizeof(buffer);
      RETURN_IF_ERROR(PrepareNotifyConnectionStatus(&reply, input));
      nearby_message_stream_Send(input->state.peer_address, &reply);
    }
  }
  return kNearbyStatusOK;
}

// |seeker| is the peer that has initiated the update.
static void UpdateAndNotifySass(const rfcomm_input* seeker) {
  if (advertisement_mode & NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE &&
      IncludeSass()) {
    NEARBY_TRACE(INFO, "Audio state changed, update advertisement");
    nearby_platform_SetAdvertisement(NULL, 0, kDisabled);
    SetNonDiscoverableAdvertisement();
  }
  NotifySassConnectionStatus(seeker);
}
static void OnAudioStateChange() { UpdateAndNotifySass(NULL); }

static char HexDigit(int digit) {
  if (digit < 10) return '0' + digit;
  if (digit < 16) return 'A' + digit - 10;
  return '?';
}

static void OnMultipointSwitchEvent(uint8_t reason, uint64_t peer_address,
                                    const char* name) {
  uint8_t buffer[MAX_MESSAGE_STREAM_PAYLOAD_SIZE];
  nearby_message_stream_Message message;
  message.message_group = MESSAGE_GROUP_SASS;
  message.message_code = MESSAGE_CODE_SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT;
  message.data = buffer;
  message.data[0] = reason;
  if (name != NULL) {
    size_t name_length = strlen(name);
    size_t max_name_length =
        sizeof(buffer) - SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT_NAME_OFFSET;
    if (name_length > max_name_length) name_length = max_name_length;
    message.length =
        SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT_NAME_OFFSET + name_length;
    memcpy(message.data + SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT_NAME_OFFSET, name,
           name_length);
  } else {
    message.length = 6;
    message.data[2] = HexDigit((peer_address >> 12) & 0xF);
    message.data[3] = HexDigit((peer_address >> 8) & 0xF);
    message.data[4] = HexDigit((peer_address >> 4) & 0xF);
    message.data[5] = HexDigit(peer_address & 0xF);
  }

  for (int i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    rfcomm_input* input = &rfcomm_inputs[i];
    if (IsSassSeeker(input)) {
      message.data[1] =
          input->state.peer_address == peer_address
              ? SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT_TARGET_THIS_DEVICE
              : SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT_TARGET_ANOTHER_DEVICE;
      nearby_message_stream_Send(input->state.peer_address, &message);
    }
  }
}

static const nearby_platform_AudioCallbacks kAudioInterface = {
    .on_state_change = OnAudioStateChange,
    .on_multipoint_switch_event = OnMultipointSwitchEvent,
};

static void PrepareNotifySassCapabilityResponse(
    nearby_message_stream_Message* message) {
  NEARBY_ASSERT(message->length >= 2 * sizeof(uint16_t));
  message->message_code = MESSAGE_CODE_SASS_NOTIFY_CAPABILITY;
  message->length = 2 * sizeof(uint16_t);
  nearby_utils_CopyBigEndian(message->data, kSassVersion, sizeof(uint16_t));
  nearby_utils_CopyBigEndian(message->data + sizeof(uint16_t),
                             nearby_fp_GetSassCapabilityFlags(),
                             sizeof(uint16_t));
}

static void PrepareNotifySwitchingPreferenceResponse(
    nearby_message_stream_Message* message) {
  NEARBY_ASSERT(message->length >= 2);
  message->message_code = MESSAGE_CODE_SASS_NOTIFY_SWITCHING_PREFERENCE;
  message->length = 2;
  message->data[0] = nearby_platform_GetSwitchingPreference();
  message->data[1] = 0;
}

// Verifies MAC and sends a NACK if verification failed.
static nearby_platform_status VerifyMac(
    uint64_t peer_address, const nearby_message_stream_Message* message,
    rfcomm_input* input) {
  nearby_platform_status status = nearby_fp_VerifyMessageAuthenticationCode(
      message->data, message->length, input->in_use_account_key,
      input->session_nonce);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(WARNING, "Invalid MAC on message %d", message->message_code);
    nearby_message_stream_SendNack(peer_address, message,
                                   FAIL_REASON_INVALID_MAC);
  }
  return status;
}

// Finds the account key which was used to encode |message| and marks it at
// the in-use key for this session
static nearby_platform_status IndicateInUseKey(
    const nearby_message_stream_Message* message, rfcomm_input* input) {
  int offset = 0;
  while (true) {
    offset = nearby_fp_GetNextUniqueAccountKeyIndex(offset);
    if (offset == -1) {
      return kNearbyStatusInvalidInput;
    } else if (kNearbyStatusOK ==
               nearby_fp_VerifyMessageAuthenticationCode(
                   message->data, message->length,
                   nearby_fp_GetAccountKey(offset)->account_key,
                   input->session_nonce)) {
      memcpy(input->in_use_account_key,
             nearby_fp_GetAccountKey(offset)->account_key,
             ACCOUNT_KEY_SIZE_BYTES);
      nearby_fp_MarkAccountKeyAsActive(offset);
      UpdateAndNotifySass(NULL);
      return kNearbyStatusOK;
    }
    offset++;
  }
}

// Selects a next, preferred audio source. The request to change the audio
// source came from |input|, which presumably is the current audio source.
static uint64_t FindNextAudioSource(const rfcomm_input* input) {
  int i;
  // 1. Select another seeker that uses the same account key.
  for (i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    if (rfcomm_inputs[i].state.peer_address != INVALID_PEER_ADDRESS &&
        rfcomm_inputs[i].state.peer_address != input->state.peer_address &&
        IsSameAccountKey(&rfcomm_inputs[i], input)) {
      return rfcomm_inputs[i].state.peer_address;
    }
  }
  // 2. Select any other seeker.
  for (i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    if (rfcomm_inputs[i].state.peer_address != INVALID_PEER_ADDRESS &&
        rfcomm_inputs[i].state.peer_address != input->state.peer_address) {
      return rfcomm_inputs[i].state.peer_address;
    }
  }
  // 3. There's only one connected seeker
  return INVALID_PEER_ADDRESS;
}

static nearby_platform_status HandleSassMessage(
    uint64_t peer_address, nearby_message_stream_Message* message) {
  rfcomm_input* input = GetRfcommInput(peer_address);
  NEARBY_ASSERT(input != NULL);
  uint8_t buffer[MAX_MESSAGE_STREAM_PAYLOAD_SIZE];
  nearby_message_stream_Message reply;
  reply.message_group = MESSAGE_GROUP_SASS;
  reply.data = buffer;
  reply.length = sizeof(buffer);
  switch (message->message_code) {
    case MESSAGE_CODE_SASS_GET_CAPABILITY:
      PrepareNotifySassCapabilityResponse(&reply);
      return nearby_message_stream_Send(peer_address, &reply);
    case MESSAGE_CODE_SASS_SET_MULTIPOINT_STATE: {
      RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 17));
      RETURN_IF_ERROR(VerifyMac(peer_address, message, input));
      bool enable = message->data[0] == 1;
      NEARBY_TRACE(INFO, "Set Multipoint enable: %d", enable);
      return SendResponse(peer_address, message,
                          nearby_platform_SetMultipoint(peer_address, enable));
    }
    case MESSAGE_CODE_SASS_SET_SWITCHING_PREFERENCE: {
      RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 18));
      RETURN_IF_ERROR(VerifyMac(peer_address, message, input));
      uint8_t flags = message->data[0];
      NEARBY_TRACE(INFO, "Set switching preference: 0x%x", flags);
      return SendResponse(peer_address, message,
                          nearby_platform_SetSwitchingPreference(flags));
    }
    case MESSAGE_CODE_SASS_GET_SWITCHING_PREFERENCE:
      PrepareNotifySwitchingPreferenceResponse(&reply);
      return nearby_message_stream_Send(peer_address, &reply);
    case MESSAGE_CODE_SASS_SWITCH_ACTIVE_AUDIO_SOURCE: {
      RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 17));
      RETURN_IF_ERROR(VerifyMac(peer_address, message, input));
      uint8_t flags = message->data[0];
      NEARBY_TRACE(INFO, "Switch active audio source: 0x%x", flags);
      uint64_t preferred_audio_source =
          ISSET(flags, 7) ? peer_address : FindNextAudioSource(input);
      return SendResponse(peer_address, message,
                          nearby_platform_SwitchActiveAudioSource(
                              peer_address, flags, preferred_audio_source));
    }
    case MESSAGE_CODE_SASS_SWITCH_BACK_AUDIO_SOURCE: {
      RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 17));
      RETURN_IF_ERROR(VerifyMac(peer_address, message, input));
      uint8_t flags = message->data[0];
      NEARBY_TRACE(INFO, "Switch back: 0x%x", flags);
      return SendResponse(
          peer_address, message,
          nearby_platform_SwitchBackAudioSource(peer_address, flags));
    }
    case MESSAGE_CODE_SASS_GET_CONNECTION_STATUS:
      PrepareNotifyConnectionStatus(&reply, input);
      return nearby_message_stream_Send(peer_address, &reply);
    case MESSAGE_CODE_SASS_NOTIFY_SASS_INITIATED_CONNECTION: {
      RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 17));
      RETURN_IF_ERROR(VerifyMac(peer_address, message, input));
      uint8_t flags = message->data[0];
      NEARBY_TRACE(INFO, "Notify SASS initiated connection: 0x%x", flags);
      return SendResponse(
          peer_address, message,
          nearby_platform_NotifySassInitiatedConnection(peer_address, flags));
    }
    case MESSAGE_CODE_SASS_IN_USE_ACCOUNT_KEY: {
      RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 22));
      NEARBY_TRACE(INFO, "Indicate in-use key");
      return SendResponse(peer_address, message,
                          IndicateInUseKey(message, input));
    }
    case MESSAGE_CODE_SASS_SEND_CUSTOM_DATA: {
      RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 17));
      RETURN_IF_ERROR(VerifyMac(peer_address, message, input));
      uint8_t data = message->data[0];
      NEARBY_TRACE(INFO, "Send custom data: 0x%x", data);
      if (GetCustomData() != data) {
        SetCustomData(data);
        UpdateAndNotifySass(input);
      }
      return SendResponse(peer_address, message, kNearbyStatusOK);
    }
    case MESSAGE_CODE_SASS_SET_DROP_CONNECTION_TARGET: {
      RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 17));
      RETURN_IF_ERROR(VerifyMac(peer_address, message, input));
      uint8_t flags = message->data[0];
      NEARBY_TRACE(INFO, "Set drop connection target: 0x%x", flags);
      return SendResponse(
          peer_address, message,
          nearby_platform_SetDropConnectionTarget(peer_address, flags));
    }
  }
  return kNearbyStatusOK;
}
#endif /* NEARBY_FP_ENABLE_SASS */

static void PrepareActiveComponentResponse(
    nearby_message_stream_Message* message) {
  NEARBY_ASSERT(message->length >= 2 * sizeof(uint16_t));
  message->message_code = MESSAGE_CODE_ACTIVE_COMPONENT_RESPONSE;
  message->length = 1;
  message->data[0] = nearby_platform_GetEarbudLeftStatus() << 1 |
                     nearby_platform_GetEarbudRightStatus();
}

static nearby_platform_status HandleGeneralMessage(
    uint64_t peer_address, nearby_message_stream_Message* message) {
  rfcomm_input* input = GetRfcommInput(peer_address);
  NEARBY_ASSERT(input != NULL);
  uint8_t buffer[MAX_MESSAGE_STREAM_PAYLOAD_SIZE];
  nearby_message_stream_Message reply;
  reply.data = buffer;
  reply.length = sizeof(buffer);
  switch (message->message_group) {
    case MESSAGE_GROUP_DEVICE_INFORMATION_EVENT: {
      reply.message_group = MESSAGE_GROUP_DEVICE_INFORMATION_EVENT;
      switch (message->message_code) {
        case MESSAGE_CODE_ACTIVE_COMPONENT_REQUEST: {
          PrepareActiveComponentResponse(&reply);
          return nearby_message_stream_Send(peer_address, &reply);
        }
        case MESSAGE_CODE_CAPABILITIES: {
          RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 1));
          uint8_t flags = message->data[0];
          NEARBY_TRACE(INFO, "Set capabilities: 0x%x", flags);
          input->capabilities = flags;
          return kNearbyStatusOK;
        }
        case MESSAGE_CODE_PLATFORM_TYPE: {
          RETURN_IF_ERROR(VerifyMessageLength(peer_address, message, 2));
          uint8_t type = message->data[0];
          uint8_t build = message->data[1];
          NEARBY_TRACE(INFO, "Set platform type: 0x%x:0x%x", type, build);
          input->platform_type = type;
          input->platform_build = build;
          return kNearbyStatusOK;
        }
      }
      break;
    }
    case MESSAGE_GROUP_DEVICE_ACTION_EVENT: {
      reply.message_group = MESSAGE_GROUP_DEVICE_ACTION_EVENT;
      switch (message->message_code) {
        case MESSAGE_CODE_RING: {
          if (message->length < 1 || message->length > 2) {
            NEARBY_TRACE(WARNING, "Invalid message(%d) length %d",
                         message->message_code, message->length);
            nearby_message_stream_SendNack(peer_address, message,
                                           /* fail reason= */ 0);
            return kNearbyStatusInvalidInput;
          }
          uint8_t command = message->data[0];
          uint16_t timeout = 0;
          if (message->length > 1) timeout = message->data[1];
          NEARBY_TRACE(INFO, "Set ring device: 0x%x %d", command, timeout);
          return SendResponse(peer_address, message,
                              nearby_platform_Ring(command, timeout * 10));
        }
      }
      break;
    }
  }
  return kNearbyStatusOK;
}

static void OnMessageReceived(uint64_t peer_address,
                              nearby_message_stream_Message* message) {
  nearby_platform_status status;
  status = HandleGeneralMessage(peer_address, message);
  if (kNearbyStatusOK != status) {
    NEARBY_TRACE(WARNING,
                 "Processing stream message failed with %d Peer address: %s",
                 status, nearby_utils_MacToString(peer_address));
    return;
  }

#ifdef NEARBY_FP_ENABLE_SASS
  if (message->message_group == MESSAGE_GROUP_SASS) {
    nearby_platform_status status = HandleSassMessage(peer_address, message);
    if (kNearbyStatusOK != status) {
      NEARBY_TRACE(WARNING, "Processing SASS message failed with %d", status);
      return;
    }
  }
#endif /* NEARBY_FP_ENABLE_SASS */
  if (client_callbacks != NULL && client_callbacks->on_event != NULL) {
    nearby_event_MessageStreamReceived payload = {
        .peer_address = peer_address,
        .message_group = message->message_group,
        .message_code = message->message_code,
        .length = message->length,
        .data = message->data};
    nearby_event_Event event = {.event_type = kNearbyEventMessageStreamReceived,
                                .payload = (uint8_t*)&payload};
    client_callbacks->on_event(&event);
  }
}

nearby_platform_status nearby_fp_client_SendMessage(
    uint64_t peer_address, const nearby_message_stream_Message* message) {
  return nearby_message_stream_Send(peer_address, message);
}

nearby_platform_status nearby_fp_client_SendAck(
    const nearby_event_MessageStreamReceived* message) {
  nearby_message_stream_Message stream_message = {
      .message_group = message->message_group,
      .message_code = message->message_code,
  };
  return nearby_message_stream_SendAck(message->peer_address, &stream_message);
}

nearby_platform_status nearby_fp_client_SendNack(
    const nearby_event_MessageStreamReceived* message, uint8_t fail_reason) {
  nearby_message_stream_Message stream_message = {
      .message_group = message->message_group,
      .message_code = message->message_code,
  };
  return nearby_message_stream_SendNack(message->peer_address, &stream_message,
                                        fail_reason);
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
static void OnBatteryChanged(void) {
  nearby_platform_status status;
  if (pairing_state != kPairingStateIdle &&
      pairing_state != kPairingStateWaitingForAccountKeyWrite &&
      pairing_state != kPairingStateWaitingForAdditionalData) {
    NEARBY_TRACE(ERROR, "%s: device is in pairing process", __func__);
    return;
  }

  if (IncludeBatteryInfo() &&
      (advertisement_mode & NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE)) {
    status = nearby_platform_SetAdvertisement(NULL, 0, kDisabled);
    if (status != kNearbyStatusOK) {
      NEARBY_TRACE(ERROR, "Failed to update battery change, status: %d",
                   status);
      return;
    }

    status = SetNonDiscoverableAdvertisement();
    if (status != kNearbyStatusOK) {
      NEARBY_TRACE(ERROR, "Failed to update battery change, status: %d",
                   status);
      return;
    }
  }

#if NEARBY_FP_MESSAGE_STREAM
  for (int i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    rfcomm_input* input = &rfcomm_inputs[i];
    if (input->state.peer_address != INVALID_PEER_ADDRESS) {
      status = SendBatteryInfoMessage(input->state.peer_address);
      if (status != kNearbyStatusOK) {
        NEARBY_TRACE(ERROR,
                     "Failed to send battery change, status: %d "
                     "Peer address: %s",
                     status,
                     nearby_utils_MacToString(input->state.peer_address));
      }
      status = SendBatteryTimeMessage(input->state.peer_address);
      if (status != kNearbyStatusOK) {
        NEARBY_TRACE(ERROR,
                     "Failed to send battery change, status: %d "
                     "Peer address: %s",
                     status,
                     nearby_utils_MacToString(input->state.peer_address));
      }
    }
  }
#endif /* NEARBY_FP_MESSAGE_STREAM */
}
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

static const nearby_platform_BleInterface kBleInterface = {
    .on_gatt_write = OnGattWrite,
    .on_gatt_read = OnGattRead,
};

static const nearby_platform_BtInterface kBtInterface = {
    .on_pairing_request = OnPairingRequest,
    .on_paired = OnPaired,
    .on_pairing_failed = OnPairingFailed,
#if NEARBY_FP_MESSAGE_STREAM
    .on_message_stream_connected = OnMessageStreamConnected,
    .on_message_stream_disconnected = OnMessageStreamDisconnected,
    .on_message_stream_received = OnMessageStreamReceived,
#endif /* NEARBY_FP_MESSAGE_STREAM */
};

#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
static nearby_platform_BatteryInterface kBatteryInterface = {
    .on_battery_changed = OnBatteryChanged,
};
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

static nearby_platform_status EnterDisabledMode() {
  nearby_platform_SetDefaultCapabilities();
  return nearby_platform_SetAdvertisement(NULL, 0, kDisabled);
}

static nearby_platform_status EnterDiscoverableMode() {
  size_t length;
  uint8_t advertisement[DISCOVERABLE_ADV_SIZE_BYTES];
  if (pairing_state != kPairingStateIdle &&
      pairing_state != kPairingStateWaitingForAccountKeyWrite &&
      pairing_state != kPairingStateWaitingForAdditionalData) {
    NEARBY_TRACE(ERROR, "%s: device is in pairing process", __func__);
    return kNearbyStatusError;
  }
  length = nearby_fp_CreateDiscoverableAdvertisement(advertisement,
                                                     sizeof(advertisement));
  length += nearby_fp_AppendTxPower(advertisement + length,
                                    sizeof(advertisement) - length,
                                    nearby_platform_GetTxLevel());
  return nearby_platform_SetAdvertisement(advertisement, length,
                                          kNoLargerThan100ms);
}

static nearby_platform_status EnterNonDiscoverableMode() {
  if (pairing_state != kPairingStateIdle &&
      pairing_state != kPairingStateWaitingForAccountKeyWrite &&
      pairing_state != kPairingStateWaitingForAdditionalData) {
    NEARBY_TRACE(ERROR, "%s: device is in pairing process", __func__);
    return kNearbyStatusError;
  }
  nearby_platform_SetDefaultCapabilities();
  return SetNonDiscoverableAdvertisement();
}

static bool ShouldRotateBleAddress(int mode) {
  if (IsInPairingMode()) {
    return false;
  }
  // We should rotate if advertisement mode changes to discoverable to prevent
  // replay attacks.
  return (mode & NEARBY_FP_ADVERTISEMENT_DISCOVERABLE) &&
         (!(advertisement_mode & NEARBY_FP_ADVERTISEMENT_DISCOVERABLE));
}

static uint32_t GetRotationDelayMs() {
  uint32_t delay_ms = ADDRESS_ROTATION_PERIOD_MS;
  // Rotation should happen every 1024 seconds on average. It is required that
  // the precise point at which the beacon starts advertising the new
  // identifier is randomized within the window. This logic should give us
  // +/-200 seconds variability.
  for (int i = 0; i < 5; i++) {
    delay_ms += (50 << i) * (int8_t)nearby_platform_Rand();
  }
  return delay_ms;
}

static void MaybeRotateBleAddress();

static void CancelAddressRotationTimer() {
  void* task = address_rotation_task;
  address_rotation_task = NULL;
  if (task != NULL) {
    nearby_platform_CancelTimer(task);
  }
}

static void ScheduleAddressRotation() {
  CancelAddressRotationTimer();
  address_rotation_task =
      nearby_platform_StartTimer(MaybeRotateBleAddress, GetRotationDelayMs());
}

static nearby_platform_status UpdateAdvertisements() {
  if (advertisement_mode == NEARBY_FP_ADVERTISEMENT_NONE) {
    return EnterDisabledMode();
  }
  if (advertisement_mode & NEARBY_FP_ADVERTISEMENT_DISCOVERABLE) {
    return EnterDiscoverableMode();
  }
  if (advertisement_mode & NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE) {
    return EnterNonDiscoverableMode();
  }
  return kNearbyStatusUnsupported;
}

static void MaybeRotateBleAddress() {
  ScheduleAddressRotation();
  if (IsInPairingMode()) {
    return;
  }
  RotateBleAddress();
  UpdateAdvertisements();
}

static bool NeedsPeriodicAddressRotation() {
  // FP spec says we should rotate BLE adress every ~15 minutes when
  // advertising
  return (advertisement_mode & (NEARBY_FP_ADVERTISEMENT_DISCOVERABLE |
                                NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE)) != 0;
}

nearby_platform_status nearby_fp_client_SetAdvertisement(int mode) {
  if (advertisement_mode == mode) {
    return kNearbyStatusOK;
  }
  if (ShouldRotateBleAddress(mode)) {
    CancelAddressRotationTimer();
    RotateBleAddress();
  }
  advertisement_mode = mode;
  if (NeedsPeriodicAddressRotation() && address_rotation_task == NULL) {
    ScheduleAddressRotation();
  }
  return UpdateAdvertisements();
}

#if NEARBY_FP_MESSAGE_STREAM
nearby_platform_status nearby_fp_client_GetSeekerInfo(
    nearby_fp_client_SeekerInfo* seeker_info, size_t* seeker_info_length) {
  int sl = *seeker_info_length;
  int inx = 0;
  *seeker_info_length = inx;
  for (int i = 0; i < NEARBY_MAX_RFCOMM_CONNECTIONS; i++) {
    if (rfcomm_inputs[i].state.peer_address != INVALID_PEER_ADDRESS) {
      if (inx >= sl) return kNearbyStatusInvalidInput;
      seeker_info[inx].peer_address = rfcomm_inputs[i].state.peer_address;
      seeker_info[inx].capabilities = rfcomm_inputs[i].capabilities;
      seeker_info[inx].platform_type = rfcomm_inputs[i].platform_type;
      seeker_info[inx].platform_build = rfcomm_inputs[i].platform_build;
      inx++;
      *seeker_info_length = inx;
    }
  }
  return kNearbyStatusOK;
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

nearby_platform_status nearby_fp_client_Init(
    const nearby_fp_client_Callbacks* callbacks) {
  nearby_platform_status status;

  client_callbacks = callbacks;
  pairing_state = kPairingStateIdle;
  pairing_failure_count = 0;
#if NEARBY_FP_MESSAGE_STREAM
  memset(rfcomm_inputs, 0, sizeof(rfcomm_inputs));
#endif /* NEARBY_FP_MESSAGE_STREAM */
  advertisement_mode = NEARBY_FP_ADVERTISEMENT_NONE;
  address_rotation_task = NULL;
  peer_public_address = 0;
  DiscardPendingAccountKey();

  status = nearby_platform_OsInit();
  if (status != kNearbyStatusOK) return status;

  status = nearby_platform_SecureElementInit();
  if (status != kNearbyStatusOK) return status;

  status = nearby_platform_BtInit(&kBtInterface);
  if (status != kNearbyStatusOK) return status;

  status = nearby_platform_BleInit(&kBleInterface);
  if (status != kNearbyStatusOK) return status;

  status = nearby_platform_PersistenceInit();
  if (status != kNearbyStatusOK) return status;

#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
  status = nearby_platform_BatteryInit(&kBatteryInterface);
  if (status != kNearbyStatusOK) return status;
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

  status = nearby_fp_LoadAccountKeys();
  if (status != kNearbyStatusOK) return status;

#ifdef NEARBY_FP_ENABLE_SASS
  SetCustomData(0);
  status = nearby_platform_AudioInit(&kAudioInterface);
  if (status != kNearbyStatusOK) return status;
#endif /* NEARBY_FP_ENABLE_SASS */

  RotateBleAddress();

  return status;
}
