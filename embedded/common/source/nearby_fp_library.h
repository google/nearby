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
#include "nearby_message_stream.h"
#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
#include "nearby_platform_battery.h"
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define BT_ADDRESS_LENGTH 6
#define DISCOVERABLE_ADV_SIZE_BYTES 10
#ifdef NEARBY_FP_ENABLE_SASS
// Sufficent for 5 account keys, battery notification and SASS info
#define NON_DISCOVERABLE_ADV_SIZE_BYTES 32
#else
// Sufficent for 5 account keys and battery notification
#define NON_DISCOVERABLE_ADV_SIZE_BYTES 25
#endif /* NEARBY_FP_ENABLE_SASS */

#define ADDITIONAL_DATA_HEADER_SIZE 16
#define HMAC_SHA256_KEY_SIZE 64
#define SHA256_KEY_SIZE 32

// Random Resolvable Field header size
#define RRF_HEADER_SIZE 1

// Field types in the non-discoverable advertisement
#define SALT_FIELD_TYPE 1
#define BATTERY_INFO_WITH_UI_INDICATION_FIELD_TYPE 3
#define BATTERY_INFO_WITHOUT_UI_INDICATION_FIELD_TYPE 4
#define SASS_ADVERTISEMENT_FIELD_TYPE 5
#define RANDOM_RESOLVABLE_FIELD_TYPE 6

// Creates advertisement with the Model Id. Returns the number of bytes
// written to |output|.
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

#if NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
void SerializeBatteryInfo(uint8_t* output,
                          const nearby_platform_BatteryInfo* battery_info);

// Creates advertisement with Account Key Data and optionally battery info.
// |battery_info| can be NULL. Returns the number of bytes written to |output|.
//
// output            - Advertisement data output buffer.
// length            - Length of output buffer.
// show_ui_indicator - Ask seeker to show UI indication.
// show_battery_indicator - Ask seeker to show battery indicator
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
void nearby_fp_CopyAccountKey(nearby_platform_AccountKeyInfo* dest,
                              unsigned key_number);

// Gets number of active keys.
size_t nearby_fp_GetAccountKeyCount();

// Gets the number of unique account keys. Account keys are duplicated when two
// or more seekers share an account key
size_t nearby_fp_GetUniqueAccountKeyCount();

// Returns the index of the next unique account key in [offset..number of keys]
// range, that is a key that wasn't already seen in [0..offset -1] range.
// Returns -1 if not found.
int nearby_fp_GetNextUniqueAccountKeyIndex(int offset);

// Gets pointer to given key by ordinal number.
//
// key_number - Ordinal number of key to return.
const nearby_platform_AccountKeyInfo* nearby_fp_GetAccountKey(
    unsigned key_number);

// Marks account key as active by moving it to the top of the key list.
//
// key_number - Original number of key to mark active.
void nearby_fp_MarkAccountKeyAsActive(unsigned key_number);

// Adds key to key list. Inserts key to top of list.
//
// key - Buffer containing key to insert.
void nearby_fp_AddAccountKey(const nearby_platform_AccountKeyInfo* key);

// Computes the account bloom filter and stores it in the Account Key Filter
// field in the advertisement. Returns the bloom filter size.
//
// `advertisement` is a non-discoverable FP advertisement. It must contain an
// LTV field for Account Key Filter, where the LT header is already set but the
// contents may be unitialized. It must contain an LTV with salt. Battery Info
// field and Random Resolvable field are used in the bloom filter calculation if
// present in the advertisement, When `use_sass_format` is set, the bloom filter
// defined by SASS will be used.
size_t nearby_fp_SetBloomFilter(uint8_t* advertisement, bool use_sass_format,
                                const uint8_t* in_use_key);

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
    uint8_t output[AES_MESSAGE_SIZE_BYTES], bool extended_response);

// Context for calculating HMAC-SHA256 hash
typedef struct {
  // The resulting hash after calling `nearby_fp_HmacSha256Finish`
  uint8_t hash[SHA256_KEY_SIZE];
  uint8_t hmac_key[HMAC_SHA256_KEY_SIZE];
} nearby_fp_HmacSha256Context;

// Starts calculating HMAC-SHA156 hash incrementally. Only one hash calculation
// can be running at a time.
//
// context - Uninitalized context.
// key        - Input key to calculate hash.
// key_length - Length of key.
nearby_platform_status nearby_fp_HmacSha256Start(
    nearby_fp_HmacSha256Context* context, const uint8_t* key,
    size_t key_length);

// Adds data to the hash.
//
// data       - Data to calculate hash.
// data_length - Data length.
nearby_platform_status nearby_fp_HmacSha256Update(const uint8_t* data,
                                                  size_t data_length);
// Finishes hash calculation. The parameters must match those passed to
// `nearby_fp_HmacSha256Start`.
//
// context - Context initalized with `nearby_fp_HmacSha256Start`.
// key        - Input key to calculate hash.
// key_length - Length of key.
nearby_platform_status nearby_fp_HmacSha256Finish(
    nearby_fp_HmacSha256Context* context, const uint8_t* key,
    size_t key_length);

// Implements HKDF-Extract method with SHA256 hash per
// https://www.rfc-editor.org/rfc/rfc5869#section-2.2
//
// out - output Pseudo Random Key material
// salt - optional salt
// salt_length - salt length in bytes. Set to 0 when `salt is NULL
// ikm - Input Key Material
// ikm_length - IKM length in bytes
nearby_platform_status nearby_fp_HkdfExtractSha256(uint8_t out[SHA256_KEY_SIZE],
                                                   const uint8_t* salt,
                                                   size_t salt_length,
                                                   const uint8_t* ikm,
                                                   size_t ikm_length);

// Implements HKDF-Expand with SHA256 hash per
// https://www.rfc-editor.org/rfc/rfc5869#section-2.3
//
// out - Output Key Material
// out_length - OKM length in bytes
// prk - Pseudo Random Key
// prk_length - PRK length in bytes
// info - optional context information
// info_length - info length in bytes. Set to 0 when `info` is NULL.
nearby_platform_status nearby_fp_HkdfExpandSha256(
    uint8_t* out, size_t out_length, const uint8_t* prk, size_t prk_length,
    const uint8_t* info, size_t info_length);

const uint8_t* nearby_fp_FindLtv(const uint8_t* advertisement, int type);

// Calculates HMAC SHA256 hash.
//
// out        - Output buffer for hash.
// key        - Input key to calculate hash.
// key_length - Length of key.
// data       - Data to calculate hash.
// data_length - Data length.
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

// Encrypts |data| in place: data = data ^ AES(key, iv). The data must
// be shorter than AES_MESSAGE_SIZE_BYTES.
// Side effect: |iv| contents are destroyed.
nearby_platform_status nearby_fp_AesEncryptIv(
    uint8_t* data, size_t length, uint8_t iv[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]);

#if NEARBY_FP_ENABLE_ADDITIONAL_DATA
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

uint8_t nearby_fp_GetSassConnectionState();

size_t nearby_fp_GenerateSassAdvertisement(
    uint8_t* advertisement, size_t length, uint8_t connection_state,
    uint8_t custom_data, const uint8_t* devices_bitmap, size_t bitmap_length);

uint16_t nearby_fp_GetSassCapabilityFlags();

nearby_platform_status nearby_fp_VerifyMessageAuthenticationCode(
    const uint8_t* message, size_t length,
    const uint8_t key[ACCOUNT_KEY_SIZE_BYTES],
    const uint8_t session_nonce[SESSION_NONCE_SIZE]);

// Encrypts Random Resolvable Field in place. The payload must start
// at RRF_HEADER_SIZE offset in the |data| buffer. |salt_field| is a
// part of non-discoverable advertisement
nearby_platform_status nearby_fp_EncryptRandomResolvableField(
    uint8_t* data, size_t length, const uint8_t key[AES_MESSAGE_SIZE_BYTES],
    const uint8_t* salt_field);
#ifdef __cplusplus
}
#endif

#endif  // NEARBY_FP_LIBRARY_H
