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

#ifndef NEARBY_FP_LIBRARY_H
#define NEARBY_FP_LIBRARY_H

// clang-format off
#include "nearby_config.h"
// clang-format on

#include "nearby.h"
#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
#include "nearby_platform_battery.h"
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define BT_ADDRESS_LENGTH 6
#define DISCOVERABLE_ADV_SIZE_BYTES 10
// Sufficient for 5 account keys and battery notification
#define NON_DISCOVERABLE_ADV_SIZE_BYTES 24

#define ADDITIONAL_DATA_HEADER_SIZE 16

// Creates advertisement with the Model Id. Returns the number of bytes written
// to |output|.
//
// output - Advertisement data output buffer.
// length - Length of output buffer.
size_t nearby_fp_CreateDiscoverableAdvertisement(uint8_t* output,
                                                 size_t length);

// Creates advertisement with Account Key Data. Returns the number of bytes
// written to |output|.
//
// output            - Advertisement data output buffer.
// length            - Length of data output buffer.
// show_ui_indicator - Ask seeker to show UI indication.
size_t nearby_fp_CreateNondiscoverableAdvertisement(
    uint8_t* output, size_t length, bool show_pairing_indicator);

#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
void SerializeBatteryInfo(uint8_t* output,
                          const nearby_platform_BatteryInfo* battery_info);

// Creates advertisement with Account Key Data and optionally battery info.
// |battery_info| can be NULL. Returns the number of bytes written to |output|.
//
// output            - Advertisement data output buffer.
// length            - Length of output buffer.
// show_ui_indicator - Ask seeker to show UI indication.
// battery_info      - Battery status data structure.
size_t nearby_fp_CreateNondiscoverableAdvertisementWithBattery(
    uint8_t* output, size_t length, bool show_pairing_indicator,
    bool show_battery_indicator,
    const nearby_platform_BatteryInfo* battery_info);
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

// Appends TX power stanza to the advertisement. |Advertisement| should point
// past the end of advertisement created with one of the 'Create*' functions.
//
// advertisement - Pointer to end of advertising data.
// length        - Remaining length of advertising data.
// power         - Power level.
size_t nearby_fp_AppendTxPower(uint8_t* advertisement, size_t length,
                               int8_t tx_power);

// Loads account keys from NV storage.
nearby_platform_status nearby_fp_LoadAccountKeys();
// Saves account keys to NV storage.
nearby_platform_status nearby_fp_SaveAccountKeys();

// Gets Specific key by number to buffer
//
// dest       - Buffer for key fetched.
// key_number - Number of key to fetch.
void nearby_fp_CopyAccountKey(uint8_t* dest, unsigned key_number);

// Gets number of active keys.
size_t nearby_fp_GetAccountKeyCount();
// Gets offset of key by key number. Returns the offset.
//
// key_number - ordinal number of key to return.
size_t nearby_fp_GetAccountKeyOffset(unsigned key_number);
// Gets pointer to given key by ordinal number.
//
// key_number - Ordinal number of key to return.
const uint8_t* nearby_fp_GetAccountKey(unsigned key_number);
// Marks account key as active by moving it to the top of the key list.
//
// key_number - Original number of key to mark active.
void nearby_fp_MarkAccountKeyAsActive(unsigned key_number);
// Adds key to key list. Inserts key to top of list.
//
// key - Buffer containing key to insert.
void nearby_fp_AddAccountKey(const uint8_t key[ACCOUNT_KEY_SIZE_BYTES]);
// Gets fast pair model ID
//
// output - Buffer to hold model ID.
// length - On input, contains the length of the buffer.
//          On output, returns the length of the model ID returned.
nearby_platform_status nearby_fp_GattReadModelId(uint8_t* output,
                                                 size_t* length);
// Creates shared secret from remote public key.
//
// remote_public_key - 512 bit peer public key.
// output            - Buffer that returns the shared secret.
nearby_platform_status nearby_fp_CreateSharedSecret(
    const uint8_t remote_public_key[64],
    uint8_t output[ACCOUNT_KEY_SIZE_BYTES]);

// Creates raw key based paring response.
//
// output - Buffer returning the pairing response.
nearby_platform_status nearby_fp_CreateRawKeybasedPairingResponse(
    uint8_t output[AES_MESSAGE_SIZE_BYTES]);

#ifdef NEARBY_FP_ENABLE_ADDITIONAL_DATA
// Calculates HMAC SHA256 hash.
//
// out        - Output buffer for hash.
// key        - Input key to calculate hash.
// key_length - Length of key.
// data       - Data to calculate hash.
// data_lenth - Data length.
nearby_platform_status nearby_fp_HmacSha256(uint8_t out[32], const uint8_t* key,
                                            size_t key_length,
                                            const uint8_t* data,
                                            size_t data_length);

// The first 8 bytes of the message are the nonce. The message is
// encrypted/decrypted in place.
//
// message        - Message buffer in/out.
// message_length - Length of message.
// key            - Key.
nearby_platform_status nearby_fp_AesCtr(
    uint8_t* message, size_t message_length,
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]);

// Decodes data package read from Additional Data characteristics. The decoding
// happens in-place. Returns an error if HMAC-SHA checksum is invalid.
//
// data   - Data to decode.
// length - Length of data.
// key    - Key.
nearby_platform_status nearby_fp_DecodeAdditionalData(
    uint8_t* data, size_t length, const uint8_t key[ACCOUNT_KEY_SIZE_BYTES]);

// Encodes data package for writing to Additional Data characteristics. The
// encoding happens in-place. The first 16 bytes of the data buffer are used for
// checksum and nonce, so the actual message must begin at offset 16.
//
// data   - Data to encode.
// length - Length of data.
// key    - Key.
nearby_platform_status nearby_fp_EncodeAdditionalData(
    uint8_t* data, size_t length, const uint8_t key[ACCOUNT_KEY_SIZE_BYTES]);

#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */

// Computes sha256 sum.
//
// out    - Output 256 bit sum.
// data   - Data to compute sha256 sum over.
// length - Length of data.
nearby_platform_status nearby_fp_Sha256(uint8_t out[32], const void* data,
                                        size_t length);

#ifdef __cplusplus
}
#endif

#endif  // NEARBY_FP_LIBRARY_H
