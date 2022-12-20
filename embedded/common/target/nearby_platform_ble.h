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

#ifndef NEARBY_PLATFORM_BLE_H
#define NEARBY_PLATFORM_BLE_H

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  kModelId = 0,       // FE2C1233-8366-4814-8EB0-01DE32100BEA (read)
  kKeyBasedPairing,   // FE2C1234-8366-4814-8EB0-01DE32100BEA (write, notify)
  kPasskey,           // FE2C1235-8366-4814-8EB0-01DE32100BEA (write, notify)
  kAccountKey,        // FE2C1236-8366-4814-8EB0-01DE32100BEA (write)
  kFirmwareRevision,  // 0x2A26
  kAdditionalData,    // FE2C1237-8366-4814-8EB0-01DE32100BEA
  // Message Stream PSM characteristic is required to establish an L2CAP
  // communication channel between Seeker and Provider. Platforms that don't
  // support L2CAP channel may choose not to implement this characteristic.
  // By default, the Seeker connect to the Provider using RFCOMM.
  kMessageStreamPsm,  // FE2C1239-8366-4814-8EB0-01DE32100BEA (read, encrypted)
} nearby_fp_Characteristic;

typedef enum {
  kDisabled,
  kNoLargerThan100ms,
  kNoLargerThan250ms
} nearby_fp_AvertisementInterval;

typedef struct {
  nearby_platform_status (*on_gatt_write)(
      uint64_t peer_address, nearby_fp_Characteristic characteristic,
      const uint8_t* request, size_t length);
  nearby_platform_status (*on_gatt_read)(
      uint64_t peer_address, nearby_fp_Characteristic characteristic,
      uint8_t* output, size_t* length);

} nearby_platform_BleInterface;

// Gets BLE address.
uint64_t nearby_platform_GetBleAddress();

// Sets BLE address. Returns address after change, which may be different than
// requested address.
//
// address - BLE address to set.
uint64_t nearby_platform_SetBleAddress(uint64_t address);

#ifdef NEARBY_FP_HAVE_BLE_ADDRESS_ROTATION
// Rotates BLE address to a random resolvable private address (RPA). Returns
// address after change.
uint64_t nearby_platform_RotateBleAddress();
#endif /* NEARBY_FP_HAVE_BLE_ADDRESS_ROTATION */

// Gets the PSM - Protocol and Service Mulitplexor - assigned to Fast Pair's
// Message Stream.
// To support Message Stream for BLE devices, Fast Pair will build and maintain
// a BLE L2CAP channel for sending and receiving messages. The PSM can be
// dynamic or fixed.
// Returns a 16 bit PSM number or a negative value on error. When a valid PSM
// number is returned, the device must be ready to accept L2CAP connections.
int32_t nearby_platform_GetMessageStreamPsm();

// Sends a notification to the connected GATT client.
//
// peer_address   - Address of peer.
// characteristic - Characteristic UUID
// message        - Message buffer to transmit.
// length         - Length of message in buffer.
nearby_platform_status nearby_platform_GattNotify(
    uint64_t peer_address, nearby_fp_Characteristic characteristic,
    const uint8_t* message, size_t length);

// Sets the Fast Pair advertisement payload and starts advertising at a given
// interval.
//
// payload  - Advertising data.
// length   - Length of data.
// interval - Advertising interval code.
nearby_platform_status nearby_platform_SetAdvertisement(
    const uint8_t* payload, size_t length,
    nearby_fp_AvertisementInterval interval);

// Initializes BLE
//
// ble_interface - GATT read and write callbacks structure.
nearby_platform_status nearby_platform_BleInit(
    const nearby_platform_BleInterface* ble_interface);

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_BLE_H */
