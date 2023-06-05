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

#include <deque>
#include <iostream>
#include <vector>

#include "fakes.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nearby.h"
#include "nearby_fp_client.h"
#include "nearby_fp_library.h"
#include "nearby_platform_ble.h"
#include "nearby_platform_os.h"
#include "nearby_platform_persistence.h"
#include "nearby_platform_se.h"
#include "nearby_spot.h"

using ::testing::ElementsAreArray;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"

#if NEARBY_FP_ENABLE_SPOT

#define APPEND_ARRAY(vector, array) \
  vector.insert(vector.end(), array, array + sizeof(array))

static constexpr uint8_t kUnathenticated = 0x80;
static constexpr uint8_t kInvalidValue = 0x81;
static constexpr uint8_t kNoUserConsent = 0x82;

static constexpr uint8_t kEphemeralKey[32] = {
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x60,
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
    0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x80, 0x81};

static constexpr uint8_t kOtherEphemeralKey[32] = {
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31,
    0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40, 0x41};

static constexpr uint8_t kOwnerKey[16] = {0x04, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
                                          0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xB0,
                                          0xB1, 0xB2, 0xB3, 0xB4};

static nearby_platform_AccountKeyInfo account_key_info;

static constexpr uint8_t kOtherAccountKey[] = {
    0x04, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// computed manually from kEphemeralKay, rotation: 10 and clock: 50000
static constexpr uint8_t kEid[20] = {0xe5, 0x12, 0x04, 0xe4, 0x39, 0xba, 0xda,
                                     0x54, 0x96, 0x4b, 0x06, 0xa5, 0x4f, 0x3a,
                                     0x0c, 0x05, 0x8e, 0x0a, 0x32, 0x4a};

// nonce is 8 * [kRandom]
static constexpr uint8_t kRandom = 0x45;

static constexpr uint8_t kAdvertisement[28] = {
    2,    1,    6,    24,   0x16, 0xAA, 0xFE, 0x40, 0xe5, 0x12,
    0x04, 0xe4, 0x39, 0xba, 0xda, 0x54, 0x96, 0x4b, 0x06, 0xa5,
    0x4f, 0x3a, 0x0c, 0x05, 0x8e, 0x0a, 0x32, 0x4a};

// Advertisement computed from kOtherEphemeralKey
static constexpr uint8_t kOtherAdvertisement[28] = {
    2,    1,    6,    24,   0x16, 0xaa, 0xfe, 0x40, 0x0f, 0x2a,
    0x11, 0xc9, 0x08, 0x2c, 0x3a, 0x94, 0xdc, 0x33, 0x7f, 0x78,
    0x39, 0x50, 0x3f, 0xd1, 0x08, 0x94, 0xf5, 0xb4};

// kEphemeralKey encrypted with AES-ECB-128 using kOwnerKey
static constexpr uint8_t kEncryptedEik[32] = {
    0x89, 0x30, 0xb0, 0xed, 0x94, 0xb1, 0x3c, 0x30, 0x69, 0x2a, 0xcb,
    0xb1, 0x1c, 0xf5, 0x50, 0x3b, 0xeb, 0x7c, 0x56, 0x8c, 0x88, 0xfe,
    0x1b, 0x95, 0x08, 0x69, 0x77, 0xca, 0xed, 0x24, 0x58, 0x5b};

// kOtherEphemeralKey encrypted with AES-ECB-128 using kOwnerKey
static constexpr uint8_t kOtherEncryptedEik[32] = {
    0x58, 0xcf, 0x86, 0x87, 0xfb, 0xe8, 0xcd, 0x33, 0x75, 0x44, 0x56,
    0x6d, 0x16, 0xe2, 0x43, 0x55, 0xfa, 0xf6, 0x60, 0xad, 0x01, 0x8b,
    0x3e, 0x37, 0x7d, 0xb4, 0x79, 0xdf, 0xe5, 0xd4, 0x1f, 0x98};

static constexpr uint8_t kRecoveryKey[8] = {0xe9, 0x1a, 0x75, 0x5b,
                                            0x6d, 0x15, 0x44, 0xf1};
static constexpr uint8_t kRingKey[8] = {0x02, 0x6d, 0xd8, 0xcc,
                                        0x60, 0xf2, 0x05, 0xb8};
static constexpr uint8_t kUnwantedTrackingProtectionKey[8] = {
    0x5c, 0x39, 0x3a, 0xb6, 0xeb, 0x24, 0x63, 0xfb};

// First 8 bytes of sha256(kOwnerKey || nonce)
static constexpr uint8_t kSha256OwnerKeyNonce[8] = {0x9c, 0x6d, 0x32, 0xf8,
                                                    0xc1, 0xe2, 0xe5, 0x9a};

// First 8 bytes of sha256(kOtherAccountKey || nonce)
static constexpr uint8_t kSha256OtherAccountHeyNonce[8] = {
    0x9c, 0x6d, 0x32, 0xf8, 0xc1, 0xe2, 0xe5, 0x9a};

// First 8 bytes of sha256(kEphemeralKey || nonce)
static constexpr uint8_t kSha256EikNonce[8] = {0x11, 0xb7, 0x0d, 0xf9,
                                               0x36, 0xa1, 0x1c, 0x69};

// First 8 bytes of hmac-sha256(kRecoveryKey || protocol version || nonce || 04
// || 08 )
static constexpr uint8_t kHmacSha256RecoveryKeyPayload[8] = {
    0x91, 0x3c, 0x77, 0xa7, 0xa7, 0x9e, 0x01, 0x3d};
// First 8 bytes of hamc-sha256(kRingKey || protocol version || nonce || 05 ||
// 12 || FF || 06 || 07 || 03)
static constexpr uint8_t kHmacSha256RingPayload[8] = {0x4c, 0x77, 0xa0, 0xae,
                                                      0x32, 0x0a, 0x29, 0xe5};

class SpotTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void SetEphemeralKey() {
    ASSERT_EQ(kNearbyStatusOK,
              nearby_platform_SaveValue(kStoredKeyEphemeralKey, kEphemeralKey,
                                        sizeof(kEphemeralKey)));
  }
  void SetOwnerKey() {
    ASSERT_EQ(kNearbyStatusOK,
              nearby_platform_SaveValue(kStoredKeyOwnerKey, kOwnerKey,
                                        sizeof(kOwnerKey)));
  }

  void SetAccountKeys() {
    nearby_fp_LoadAccountKeys();
    memcpy(&(account_key_info.account_key), kOwnerKey, sizeof(kOwnerKey));
    nearby_fp_AddAccountKey(&account_key_info);
    memcpy(&(account_key_info.account_key), kOtherAccountKey,
           sizeof(kOtherAccountKey));
    nearby_fp_AddAccountKey(&account_key_info);
  }

  void ReadNonce() {
    uint8_t BeaconAction[9];
    size_t length = sizeof(BeaconAction);

    EXPECT_EQ(kNearbyStatusOK, nearby_test_fakes_GattRead(
                                   kBeaconActions, BeaconAction, &length));
    // Check for SPOT beacon major protocol version to be 1
    EXPECT_EQ(SPOT_BEACON_PROTOCOL_MAJOR_VERSION, BeaconAction[0]);
    memcpy(nonce_, &BeaconAction[1], sizeof(nonce_));
    EXPECT_EQ(length, (sizeof(nonce_) + 1));
  }

  bool HasSentNotification() {
    return nearby_test_fakes_GetGattNotifications().end() !=
           nearby_test_fakes_GetGattNotifications().find(kBeaconActions);
  }
  uint8_t nonce_[8];
};

static void OnRingStateChange() { nearby_spot_OnRingStateChange(); }
static constexpr nearby_platform_OsInterface kOsInterface = {
    .on_ring_state_change = OnRingStateChange,
};

static nearby_platform_status OnGattWrite(
    uint64_t peer_address, nearby_fp_Characteristic characteristic,
    const uint8_t* request, size_t length) {
  EXPECT_EQ(kBeaconActions, characteristic);
  return nearby_spot_WriteBeaconAction(peer_address, request, length);
}
static nearby_platform_status OnGattRead(
    uint64_t peer_address, nearby_fp_Characteristic characteristic,
    uint8_t* output, size_t* length) {
  EXPECT_EQ(kBeaconActions, characteristic);
  return nearby_spot_ReadBeaconAction(peer_address, output, length);
}
static constexpr nearby_platform_BleInterface kBleInterface = {
    .on_gatt_write = OnGattWrite,
    .on_gatt_read = OnGattRead,
};

void SpotTest::SetUp() {
  nearby_platform_PersistenceInit();
  nearby_platform_SecureElementInit();
  nearby_platform_OsInit(&kOsInterface);
  nearby_platform_BleInit(&kBleInterface);
  nearby_test_fakes_SetRandomNumber(kRandom);
  SetAccountKeys();
}

TEST_F(SpotTest, Init) { ASSERT_EQ(kNearbyStatusOK, nearby_spot_Init()); }

TEST_F(SpotTest, SetAdvertisement_NoEphemeralKey_Fails) {
  nearby_spot_Init();

  ASSERT_EQ(kNearbyStatusError, nearby_spot_SetAdvertisement(true));
}

TEST_F(SpotTest, SetAdvertisement_NoOwnerKey_Fails) {
  SetEphemeralKey();
  nearby_spot_Init();

  ASSERT_EQ(kNearbyStatusError, nearby_spot_SetAdvertisement(true));
}

TEST_F(SpotTest, SetAdvertisement) {
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();

  ASSERT_EQ(kNearbyStatusOK, nearby_spot_SetAdvertisement(true));
  ASSERT_THAT(kAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
}

TEST_F(SpotTest, SetBeaconAccountKey) {
  constexpr uint8_t kAccountKey[16] = {0x04, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4,
                                       0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xD0,
                                       0xD1, 0xD2, 0xD3, 0xD4};
  uint8_t result[sizeof(kAccountKey)];
  size_t length = sizeof(result);

  ASSERT_EQ(kNearbyStatusOK, nearby_spot_SetBeaconAccountKey(kAccountKey));

  ASSERT_EQ(kNearbyStatusOK,
            nearby_platform_LoadValue(kStoredKeyOwnerKey, result, &length));
  ASSERT_THAT(result,
              ElementsAreArray(kAccountKey, kAccountKey + sizeof(kAccountKey)));
}

TEST_F(SpotTest, ReadBeaconAction) {
  std::vector<uint8_t> random_numbers = {0, 1, 2, 3, 4, 5, 6, 7};
  nearby_test_fakes_SetRandomNumberSequence(random_numbers);

  ReadNonce();

  ASSERT_THAT(nonce_, ElementsAreArray(random_numbers));
}

TEST_F(SpotTest, ReadBeaconParameters_ValidAccountKey_SendsNotification) {
  constexpr uint8_t kRequest[] = {0,    8,    0x83, 0x8b, 0x57,
                                  0x68, 0x30, 0xb8, 0x11, 0xcc};
  constexpr uint8_t kExpectedResponse[] = {
      0,    24,   0xae, 0xb6, 0x4a, 0x5d, 0xdd, 0x85, 0xec,
      0x37, 0xA8, 0x89, 0x32, 0xF1, 0x5C, 0x13, 0xDC, 0xBA,
      0xCF, 0x2C, 0x29, 0x4E, 0x0D, 0x08, 0x10, 0x83};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK, nearby_test_fakes_GattWrite(
                                   kBeaconActions, kRequest, sizeof(kRequest)));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(kExpectedResponse,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest, ReadBeaconParameters_InvalidAccountKey_RejectsRequest) {
  constexpr uint8_t kRequest[] = {0,    8,    0xFF, 0x6d, 0x32,
                                  0xf8, 0xc1, 0xe2, 0xe5, 0x9a};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kUnathenticated, nearby_test_fakes_GattWrite(
                                   kBeaconActions, kRequest, sizeof(kRequest)));

  ASSERT_FALSE(HasSentNotification());
}

TEST_F(SpotTest,
       ReadProvisioningState_OwnerKeyProvisionedDevice_SendsNotification) {
  constexpr uint8_t kRequest[] = {1,    8,    0x3f, 0xcf, 0x0f,
                                  0x68, 0x1c, 0x54, 0xee, 0xee};
  std::vector<uint8_t> expected_response = {1,    29,   0xa8, 0x51, 0x8b, 0x21,
                                            0xf3, 0x32, 0x40, 0x07, 0x03};
  APPEND_ARRAY(expected_response, kEid);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK, nearby_test_fakes_GattWrite(
                                   kBeaconActions, kRequest, sizeof(kRequest)));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(
    SpotTest,
    ReadProvisioningState_ValidNonOwnerKeyProvisionedDevice_SendsNotification) {
  constexpr uint8_t kRequest[] = {1,    8,    0xff, 0xdb, 0x61,
                                  0x2f, 0x65, 0xdc, 0xfd, 0x0a};
  std::vector<uint8_t> expected_response = {1,    29,   0x63, 0x09, 0xa7, 0x09,
                                            0x47, 0xd7, 0x05, 0x09, 0x01};
  APPEND_ARRAY(expected_response, kEid);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK, nearby_test_fakes_GattWrite(
                                   kBeaconActions, kRequest, sizeof(kRequest)));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest,
       ReadProvisioningState_OwnerKeyNotProvisionedDevice_SendsNotification) {
  constexpr uint8_t kRequest[] = {1,    8,    0x3f, 0xcf, 0x0f,
                                  0x68, 0x1c, 0x54, 0xee, 0xee};
  std::vector<uint8_t> expected_response = {1,    9,    0xae, 0xe3, 0x28, 0x42,
                                            0x58, 0xe7, 0xb1, 0xbf, 0x02};
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK, nearby_test_fakes_GattWrite(
                                   kBeaconActions, kRequest, sizeof(kRequest)));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest, ReadProvisioningState_InvalidKey_RejectsRequest) {
  constexpr uint8_t kRequest[] = {1,    8,    0xFF, 0x5a, 0x68,
                                  0x11, 0x14, 0xb1, 0x90, 0x63};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kUnathenticated, nearby_test_fakes_GattWrite(
                                   kBeaconActions, kRequest, sizeof(kRequest)));

  ASSERT_FALSE(HasSentNotification());
}

TEST_F(SpotTest, SetEid_OwnerKeyNotProvisionedDevice_StartsAdvertising) {
  std::vector<uint8_t> request = {2,    40,   0xcd, 0xbc, 0xc8,
                                  0xbc, 0x5b, 0xbd, 0x0d, 0x82};
  std::vector<uint8_t> expected_response = {2,    8,    0xf0, 0x7b, 0x29,
                                            0x79, 0x3a, 0x8f, 0xb3, 0xae};
  APPEND_ARRAY(request, kEncryptedEik);
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));

  ASSERT_THAT(kAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
}

TEST_F(SpotTest, ResetEid_ValidRequest_AdvertisesNewKey) {
  std::vector<uint8_t> request = {2,    48,   0x37, 0x40, 0x91,
                                  0xc9, 0x22, 0x49, 0x84, 0xf7};
  std::vector<uint8_t> expected_response = {2,    8,    0xf0, 0x7b, 0x29,
                                            0x79, 0x3a, 0x8f, 0xb3, 0xae};
  APPEND_ARRAY(request, kOtherEncryptedEik);
  APPEND_ARRAY(request, kSha256EikNonce);
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));

  ASSERT_THAT(kOtherAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
}

TEST_F(SpotTest, ResetEid_InvalidRequest_RejectsRequest) {
  std::vector<uint8_t> request = {2,    48,   0x37, 0x40, 0x91,
                                  0xc9, 0x22, 0x49, 0x84, 0xf7};
  constexpr uint8_t kInvalidSha256EikNonce[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  APPEND_ARRAY(request, kOtherEncryptedEik);
  APPEND_ARRAY(request, kInvalidSha256EikNonce);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kUnathenticated,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));
}

TEST_F(SpotTest, ResetEid_DeviceNotProvisioned_RejectsRequest) {
  std::vector<uint8_t> request = {2,    48,   0x37, 0x40, 0x91,
                                  0xc9, 0x22, 0x49, 0x84, 0xf7};
  APPEND_ARRAY(request, kOtherEncryptedEik);
  APPEND_ARRAY(request, kSha256EikNonce);
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_NE(kNearbyStatusOK,
            nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                        request.size()));

  ASSERT_FALSE(HasSentNotification());
  ASSERT_EQ(0, nearby_test_fakes_GetSpotAdvertisement().size());
}

TEST_F(SpotTest, SetEid_OwnerKeyProvisionedDevice_RejectsRequest) {
  std::vector<uint8_t> request = {2,    40,   0xcd, 0xbc, 0xc8,
                                  0xbc, 0x5b, 0xbd, 0x0d, 0x82};
  APPEND_ARRAY(request, kEncryptedEik);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kInvalidValue,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
  ASSERT_EQ(0, nearby_test_fakes_GetSpotAdvertisement().size());
}

TEST_F(SpotTest, SetEid_OtherkeyNotProvisionedDevice_RejectsRequest) {
  std::vector<uint8_t> request = {2,    40,   0xc9, 0x5a, 0x68,
                                  0x11, 0x14, 0xb1, 0x90, 0x63};
  APPEND_ARRAY(request, kEncryptedEik);
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kUnathenticated,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
  ASSERT_EQ(0, nearby_test_fakes_GetSpotAdvertisement().size());
}

TEST_F(SpotTest, ClearEid_OwnerKeyValidRequest_StopsAdvertising) {
  std::vector<uint8_t> request = {3,    16,   0x51, 0x86, 0x16,
                                  0x90, 0x5d, 0x86, 0x43, 0xf6};
  std::vector<uint8_t> expected_response = {3,    8,    0xa1, 0x49, 0x0c,
                                            0xe1, 0xde, 0xd4, 0xec, 0x89};
  APPEND_ARRAY(request, kSha256EikNonce);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  ASSERT_EQ(kNearbyStatusOK, nearby_spot_SetAdvertisement(true));

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
  ASSERT_EQ(0, nearby_test_fakes_GetSpotAdvertisement().size());
#if NEARBY_SPOT_FACTORY_RESET_DEVICE_ON_CLEARING_EIK
  ASSERT_EQ(0, nearby_fp_GetAccountKeyCount());
#endif
  ASSERT_EQ(kNearbyStatusError, nearby_spot_SetAdvertisement(true));
}

TEST_F(SpotTest, ClearEid_OtherKeyValidRequest_RejectsRequest) {
  std::vector<uint8_t> request = {3,    16,   0xc9, 0x5a, 0x68,
                                  0x11, 0x14, 0xb1, 0x90, 0x63};
  APPEND_ARRAY(request, kSha256EikNonce);
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  ASSERT_EQ(kNearbyStatusOK, nearby_spot_SetAdvertisement(true));

  ASSERT_THAT(kUnathenticated,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
  ASSERT_THAT(kAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
}

TEST_F(SpotTest, ClearEid_OwnerKeyInvalidRequest_RejectsRequest) {
  std::vector<uint8_t> request = {3,    16,   0x51, 0x86, 0x16, 0x90,
                                  0x5d, 0x86, 0x43, 0xf6, 0,    1,
                                  2,    3,    4,    5,    6,    7};
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  ASSERT_EQ(kNearbyStatusOK, nearby_spot_SetAdvertisement(true));

  ASSERT_THAT(kUnathenticated,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
  ASSERT_THAT(kAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
}

TEST_F(SpotTest, ReadEik_ValidRequestInPairingMode_SendsNotification) {
  std::vector<uint8_t> request = {4, 8};
  APPEND_ARRAY(request, kHmacSha256RecoveryKeyPayload);
  std::vector<uint8_t> expected_response = {4,    40,   0xec, 0x7e, 0xab,
                                            0xbd, 0x90, 0x03, 0x3b, 0xbf};
  APPEND_ARRAY(expected_response, kEncryptedEik);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  nearby_test_fakes_SetInPairingMode(true);
  nearby_test_fakes_SetHasUserContentForReadingEik(false);

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest, ReadEik_ValidRequestInEIKRecoveryMode_SendsNotification) {
  std::vector<uint8_t> request = {4, 8};
  APPEND_ARRAY(request, kHmacSha256RecoveryKeyPayload);
  std::vector<uint8_t> expected_response = {4,    40,   0xec, 0x7e, 0xab,
                                            0xbd, 0x90, 0x03, 0x3b, 0xbf};
  APPEND_ARRAY(expected_response, kEncryptedEik);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  nearby_test_fakes_SetInPairingMode(false);
  nearby_test_fakes_SetHasUserContentForReadingEik(true);

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest, ReadEik_ValidRequestNoConsent_RejectsRequest) {
  std::vector<uint8_t> request = {4, 8};
  APPEND_ARRAY(request, kHmacSha256RecoveryKeyPayload);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  nearby_test_fakes_SetInPairingMode(false);
  nearby_test_fakes_SetHasUserContentForReadingEik(false);

  ASSERT_THAT(kNoUserConsent,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
}

TEST_F(SpotTest, ReadEik_InvalidRequestHasUserConsent_RejectsRequest) {
  std::vector<uint8_t> request = {4, 8, 1, 2, 3, 4, 5, 6, 7, 8};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  nearby_test_fakes_SetHasUserContentForReadingEik(true);

  ASSERT_THAT(kUnathenticated,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
}

TEST_F(SpotTest, Ring_StartRingingRequest_CallsHal) {
  constexpr uint8_t kCommand = 0xFF;
  constexpr uint8_t kTimeoutHigh = 6;
  constexpr uint8_t kTimeoutLow = 7;
  constexpr nearby_platform_RingingVolume kRingingVolume = kRingingVolumeHigh;
  std::vector<uint8_t> request = {5,    12,   0x97, 0xcb, 0x73,
                                  0x01, 0xca, 0x3b, 0x38, 0x8b};
  request.push_back(kCommand);
  request.push_back(kTimeoutHigh);
  request.push_back(kTimeoutLow);
  request.push_back(kRingingVolume);
  nearby_platform_RingingInfo ringing_info;
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
  ASSERT_EQ(kNearbyStatusOK, nearby_platform_GetRingingInfo(&ringing_info));
  ASSERT_EQ(kCommand, ringing_info.components);
  ASSERT_EQ(kTimeoutHigh * 256 + kTimeoutLow, ringing_info.timeout);
  ASSERT_EQ(kRingingVolume, ringing_info.volume);
}

TEST_F(SpotTest, Ring_DeviceNotProvisioned_RejectsRequest) {
  constexpr uint8_t kCommand = 0xFF;
  constexpr uint8_t kTimeoutHigh = 6;
  constexpr uint8_t kTimeoutLow = 7;
  constexpr nearby_platform_RingingVolume kRingingVolume = kRingingVolumeHigh;
  std::vector<uint8_t> request = {5, 12};
  APPEND_ARRAY(request, kHmacSha256RingPayload);
  request.push_back(kCommand);
  request.push_back(kTimeoutHigh);
  request.push_back(kTimeoutLow);
  request.push_back(kRingingVolume);
  nearby_platform_RingingInfo ringing_info;
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kInvalidValue,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
  ASSERT_EQ(kNearbyStatusOK, nearby_platform_GetRingingInfo(&ringing_info));
  ASSERT_EQ(0, ringing_info.components);
  ASSERT_EQ(0, ringing_info.timeout);
  ASSERT_EQ(0, ringing_info.volume);
}

TEST_F(SpotTest, Ring_InvalidRequest_RejectsRequest) {
  std::vector<uint8_t> request = {5, 12, 1, 2,    3, 4,  5,
                                  6, 7,  8, 0xFF, 9, 10, 05};
  nearby_platform_RingingInfo ringing_info;
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kUnathenticated,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
  ASSERT_EQ(kNearbyStatusOK, nearby_platform_GetRingingInfo(&ringing_info));
  ASSERT_EQ(0, ringing_info.components);
  ASSERT_EQ(0, ringing_info.timeout);
}

TEST_F(SpotTest, Ring_StopRequest_CallsHal) {
  constexpr uint8_t kCommand = 0x00;
  std::vector<uint8_t> request = {5,    9,    0xdf, 0xf9, 0x99,
                                  0xb0, 0x82, 0xfa, 0xb2, 0x90};
  request.push_back(kCommand);
  nearby_platform_RingingInfo ringing_info;
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
  ASSERT_EQ(kNearbyStatusOK, nearby_platform_GetRingingInfo(&ringing_info));
  ASSERT_EQ(kCommand, ringing_info.components);
}

TEST_F(SpotTest, ReadRingingState_ValidRequest_SendsNotification) {
  constexpr uint8_t kNumComponents = 3;
  constexpr uint8_t kComponents = 0x02;
  constexpr uint8_t kTimeoutHigh = 6;
  constexpr uint8_t kTimeoutLow = 7;
  constexpr nearby_platform_RingingVolume kRingingVolume = kRingingVolumeHigh;

  std::vector<uint8_t> request = {6,    8,    0x97, 0xde, 0xff,
                                  0xec, 0x0f, 0xa7, 0x1c, 0xce};
  constexpr nearby_platform_RingingInfo kRingingInfo = {
      .ring_state = kRingStateStarted,
      .num_components = kNumComponents,
      .components = kComponents,
      .timeout = kTimeoutHigh * 256 + kTimeoutLow,
      .volume = kRingingVolume};
  std::vector<uint8_t> expected_response = {
      6,    11,   0x61, 0x4f,        0x2e,         0x92,       0x97,
      0xbb, 0xe6, 0x33, kComponents, kTimeoutHigh, kTimeoutLow};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  nearby_test_fakes_SetRingingInfo(&kRingingInfo);

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest, ReadRingingState_InvalidRequest_RejectsRequest) {
  std::vector<uint8_t> request = {6, 8, 1, 2, 3, 4, 5, 6, 7, 8};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kUnathenticated,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
}

TEST_F(SpotTest, OnRingStateChanged_SendsNotification) {
  constexpr uint8_t kNumComponents = 0x03;
  constexpr uint8_t kComponents = 0x02;
  constexpr uint8_t kTimeoutHigh = 6;
  constexpr uint8_t kTimeoutLow = 7;
  constexpr nearby_platform_RingingVolume kRingingVolume = kRingingVolumeHigh;

  constexpr nearby_platform_RingingInfo kRingingInfo = {
      .ring_state = kRingStateStarted,
      .num_components = kNumComponents,
      .components = kComponents,
      .timeout = kTimeoutHigh * 256 + kTimeoutLow,
      .volume = kRingingVolume};

  std::vector<uint8_t> expected_response = {5,
                                            12,
                                            0x24,
                                            0xe2,
                                            0x18,
                                            0xff,
                                            0xd0,
                                            0x16,
                                            0x23,
                                            0x43,
                                            kRingStateStarted,
                                            kComponents,
                                            kTimeoutHigh,
                                            kTimeoutLow};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  nearby_test_fakes_SetRingingInfo(&kRingingInfo);

  nearby_test_fakes_NotifyRingStateChanged();

  ASSERT_TRUE(HasSentNotification());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest, ActivateUnwantedTrackingProtectionMode_InValidRequest) {
  std::vector<uint8_t> request = {7, 8, 1, 2, 3, 4, 5, 6, 7, 8};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kUnathenticated,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(nearby_spot_GetUnwantedTrackingProtectionModeState());
  ASSERT_FALSE(HasSentNotification());
}

TEST_F(SpotTest, ActivateUnwantedTrackingProtectionMode_ValidRequest) {
  std::vector<uint8_t> request = {7,    8,    0x23, 0x9f, 0x28,
                                  0xb8, 0xba, 0xd4, 0xf4, 0x11};
  std::vector<uint8_t> expected_response = {7,    8,    0xc1, 0xfe, 0xe9,
                                            0x33, 0x9b, 0x90, 0x6d, 0x1f};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_TRUE(nearby_spot_GetUnwantedTrackingProtectionModeState());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest,
       ActivateUnwantedTrackingProtectionModeWithControlFlags_ValidRequest) {
  std::vector<uint8_t> request = {7,    9,    0x3b, 0xb4, 0x8e, 0x19,
                                  0x91, 0x39, 0xd2, 0x0e, 0x01};
  std::vector<uint8_t> expected_response = {7,    8,    0xc1, 0xfe, 0xe9,
                                            0x33, 0x9b, 0x90, 0x6d, 0x1f};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(HasSentNotification());
  ASSERT_TRUE(nearby_spot_GetUnwantedTrackingProtectionModeState());
  // Check the skip ringing authentication flag is set
  ASSERT_TRUE(nearby_spot_GetControlFlags() == 0x01);
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest, Ring_StartRingingRequest_InvalidAuthenticationData) {
  constexpr uint8_t kCommand = 0xFF;
  constexpr uint8_t kTimeoutHigh = 6;
  constexpr uint8_t kTimeoutLow = 7;
  constexpr nearby_platform_RingingVolume kRingingVolume = kRingingVolumeHigh;
  std::vector<uint8_t> request = {5,    12,   0x12, 0x34, 0x56,
                                  0x78, 0x90, 0x11, 0x12, 0x34};
  request.push_back(kCommand);
  request.push_back(kTimeoutHigh);
  request.push_back(kTimeoutLow);
  request.push_back(kRingingVolume);
  nearby_platform_RingingInfo ringing_info;
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  nearby_spot_SetUnwantedTrackingProtectionMode(true);

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_FALSE(HasSentNotification());
  // Check the skip ringing authentication flag is set
  ASSERT_TRUE(nearby_spot_GetControlFlags() == 0x01);
  ASSERT_EQ(kNearbyStatusOK, nearby_platform_GetRingingInfo(&ringing_info));
  ASSERT_EQ(kCommand, ringing_info.components);
  ASSERT_EQ(kTimeoutHigh * 256 + kTimeoutLow, ringing_info.timeout);
  ASSERT_EQ(kRingingVolume, ringing_info.volume);
}

TEST_F(SpotTest, DeactivateUnwantedTrackingProtectionMode_InValidRequest) {
  std::vector<uint8_t> request = {8, 16, 1,  2,  3,  4,  5,  6,  7,
                                  8, 9,  10, 11, 12, 13, 14, 15, 16};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  nearby_spot_SetUnwantedTrackingProtectionMode(true);

  ASSERT_THAT(kUnathenticated,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(nearby_spot_GetUnwantedTrackingProtectionModeState());
  ASSERT_FALSE(HasSentNotification());
}

TEST_F(SpotTest, DectivateUnwantedTrackingProtectionMode_ValidRequest) {
  std::vector<uint8_t> request = {8,    16,   0x7b, 0x87, 0xef,
                                  0x56, 0xb5, 0x13, 0x9c, 0xb8};
  std::vector<uint8_t> expected_response = {8,    8,    0xc7, 0x7e, 0x53,
                                            0x7c, 0x38, 0x82, 0xf7, 0xc2};
  APPEND_ARRAY(request, kSha256EikNonce);
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kNearbyStatusOK,
              nearby_test_fakes_GattWrite(kBeaconActions, request.data(),
                                          request.size()));

  ASSERT_TRUE(HasSentNotification());
  // Check the skip ringing authentication flag is reset
  ASSERT_FALSE(nearby_spot_GetControlFlags());
  ASSERT_FALSE(nearby_spot_GetUnwantedTrackingProtectionModeState());
  ASSERT_THAT(expected_response,
              ElementsAreArray(
                  nearby_test_fakes_GetGattNotifications().at(kBeaconActions)));
}

TEST_F(SpotTest, ReuseNonce_RejectsSecondRequest) {
  constexpr uint8_t kRequest[] = {1,    8,    0x3f, 0xcf, 0x0f,
                                  0x68, 0x1c, 0x54, 0xee, 0xee};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();
  ASSERT_THAT(kNearbyStatusOK, nearby_test_fakes_GattWrite(
                                   kBeaconActions, kRequest, sizeof(kRequest)));

  ASSERT_THAT(kUnathenticated, nearby_test_fakes_GattWrite(
                                   kBeaconActions, kRequest, sizeof(kRequest)));
}

TEST_F(SpotTest, RejectEmptyRequest) {
  constexpr uint8_t kRequest[] = {};
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();
  ReadNonce();

  ASSERT_THAT(kInvalidValue, nearby_test_fakes_GattWrite(
                                 kBeaconActions, kRequest, sizeof(kRequest)));
}

TEST_F(SpotTest, GetSecp160r1PublicKey) {
  constexpr uint8_t kInput[32] = {
      0x1B, 0xB4, 0xAE, 0x82, 0x37, 0x95, 0x62, 0x1B, 0xF7, 0x90, 0x4B,
      0x00, 0x13, 0x59, 0x43, 0xD0, 0x0E, 0x7D, 0x06, 0xC8, 0x72, 0x1C,
      0x75, 0x74, 0xF4, 0x6D, 0x0C, 0xF2, 0x33, 0x3A, 0x5F, 0x88};
  uint8_t output[20];
  uint8_t hashed_field = 0;
  constexpr uint8_t kExpectedOutput[20] = {
      0xB6, 0x63, 0x51, 0x83, 0x89, 0xF7, 0xE1, 0x4C, 0xEB, 0x76,
      0x20, 0xB1, 0xFB, 0xB2, 0xB7, 0x1A, 0xDD, 0x0E, 0x10, 0xC4};

  ASSERT_EQ(kNearbyStatusOK, nearby_platform_GetSecp160r1PublicKey(
                                 kInput, output, &hashed_field));
  ASSERT_THAT(kExpectedOutput,
              ElementsAreArray(output, output + sizeof(output)));
}

TEST_F(SpotTest, GetSecp160r1HashedFlagMask) {
  constexpr uint8_t kInput[32] = {
      0x97, 0x35, 0xDC, 0x6B, 0xAB, 0xFA, 0x21, 0x14, 0xA6, 0x7D, 0x7D,
      0x7B, 0x01, 0xE6, 0xBD, 0x44, 0xEF, 0xE5, 0x9B, 0x58, 0xBD, 0x65,
      0x24, 0x71, 0x38, 0xBD, 0x4A, 0xE7, 0x38, 0x33, 0x7A, 0x96};

  uint8_t output[20];
  uint8_t hashed_field = 0;
  constexpr uint8_t kExpectedHashed = 0x51;

  ASSERT_EQ(kNearbyStatusOK, nearby_platform_GetSecp160r1PublicKey(
                                 kInput, output, &hashed_field));

  ASSERT_THAT(kExpectedHashed, hashed_field);
}

#ifdef NEARBY_SPOT_BATTERY_LEVEL_INDICATION
TEST_F(SpotTest, SetAdvertisementWithNormalBattery) {
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusOK);
  nearby_test_fakes_SetMainBatteryLevel(60);
  uint8_t kExpectedAdvertisement[29];
  memcpy(kExpectedAdvertisement, kAdvertisement, 28);
  kExpectedAdvertisement[28] = 0x8E;
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();

  ASSERT_EQ(kNearbyStatusOK, nearby_spot_SetAdvertisement(true));
  ASSERT_THAT(kExpectedAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
}

TEST_F(SpotTest, SetAdvertisementWithLowBattery) {
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusOK);
  nearby_test_fakes_SetMainBatteryLevel(20);
  uint8_t kExpectedAdvertisement[29];
  memcpy(kExpectedAdvertisement, kAdvertisement, 28);
  kExpectedAdvertisement[28] = 0x88;
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();

  ASSERT_EQ(kNearbyStatusOK, nearby_spot_SetAdvertisement(true));
  ASSERT_THAT(kExpectedAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
}

TEST_F(SpotTest, SetAdvertisementWithCriticallyLowBattery) {
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusOK);
  nearby_test_fakes_SetMainBatteryLevel(10);
  uint8_t kExpectedAdvertisement[29];
  memcpy(kExpectedAdvertisement, kAdvertisement, 28);
  kExpectedAdvertisement[28] = 0x8A;
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();

  ASSERT_EQ(kNearbyStatusOK, nearby_spot_SetAdvertisement(true));
  ASSERT_THAT(kExpectedAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
}

TEST_F(SpotTest,
       SetAdvertisementWithUnwantedTrackingProtectionAndNormalBattery) {
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusOK);
  nearby_test_fakes_SetMainBatteryLevel(60);
  uint8_t kExpectedAdvertisement[29];
  memcpy(kExpectedAdvertisement, kAdvertisement, 28);
  // For Unwanted Tracking protection frame type is 0x41
  kExpectedAdvertisement[7] = 0x41;
  kExpectedAdvertisement[28] = 0x8F;
  SetEphemeralKey();
  SetOwnerKey();
  nearby_spot_Init();

  nearby_spot_SetUnwantedTrackingProtectionMode(true);
  ASSERT_EQ(kNearbyStatusOK, nearby_spot_SetAdvertisement(true));
  ASSERT_THAT(kExpectedAdvertisement,
              ElementsAreArray(nearby_test_fakes_GetSpotAdvertisement()));
}
#endif /* NEARBY_SPOT_BATTERY_LEVEL_INDICATION */

#if NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES
// Case NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES=1 Unwanted Tracking Protection
// Mode=On
TEST_F(SpotTest, SetAdvertisementMultipleAddressOnUnwantedTrackingOn) {
  uint64_t firstSpotAddress, secondSpotAddress, thirdSpotAddress,
      fourthSpotAddress, fifthSpotAddress;
  uint64_t firstBleAddress, secondBleAddress, thirdBleAddress, fourthBleAddress,
      fifthBleAddress;

  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetRandomNumber(31);
  SetEphemeralKey();
  SetOwnerKey();
  // We need to reinitialize SPOT with the new ephemeral id.
  nearby_spot_Init();
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_SPOT);

  // 1) Initial. SPOT and BLE addresses should be same
  firstSpotAddress = nearby_test_fakes_GetSpotAddress();
  firstBleAddress = nearby_platform_GetBleAddress();
  printf("firstSpotAddress=0x%lx firstBleAddress=0x%lx \n", firstSpotAddress,
         firstBleAddress);
  ASSERT_EQ(firstSpotAddress, firstBleAddress);
  nearby_spot_SetUnwantedTrackingProtectionMode(true);

  // 2) At 20 min. SPOT and BLE addresses should be different
  unsigned int next_time_ms = (20 * 60 * 1000);
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  secondSpotAddress = nearby_test_fakes_GetSpotAddress();
  secondBleAddress = nearby_platform_GetBleAddress();
  printf("secondSpotAddress=0x%lx secondBleAddress=0x%lx \n", secondSpotAddress,
         secondBleAddress);
  ASSERT_NE(secondSpotAddress, secondBleAddress);

  // 3) At 40 min. BLE address should be different than 2)
  nearby_test_fakes_SetRandomNumber(32);
  next_time_ms = (40 * 60 * 1000);  // 40min
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  thirdBleAddress = nearby_platform_GetBleAddress();
  printf("secondBleAddress=0x%lx thirdBleAddress=0x%lx \n", secondBleAddress,
         thirdBleAddress);
  ASSERT_NE(secondBleAddress, thirdBleAddress);

  // 4) At 65 min. BLE address should be different than 3)
  nearby_test_fakes_SetRandomNumber(33);
  next_time_ms = (65 * 60 * 1000);  // 65min
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  fourthBleAddress = nearby_platform_GetBleAddress();
  printf("thirdBleAddress=0x%lx fourthBleAddress=0x%lx \n", secondBleAddress,
         fourthBleAddress);
  ASSERT_NE(thirdBleAddress, fourthBleAddress);

  // 5) At 3 hr. SPOT address should be same as 2) as it's under 24 hr
  //  BLE address should be different than 4)
  nearby_test_fakes_SetRandomNumber(34);
  next_time_ms = (3 * 60 * 60 * 1000);  // 3 hr
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  fifthBleAddress = nearby_platform_GetBleAddress();
  thirdSpotAddress = nearby_test_fakes_GetSpotAddress();
  printf("thirdSpotAddress=0x%lx fifthBleAddress=0x%lx\n", thirdSpotAddress,
         fifthBleAddress);
  ASSERT_EQ(secondSpotAddress, thirdSpotAddress);
  ASSERT_NE(fourthBleAddress, fifthBleAddress);

  // 6) At 24hr+2min. SPOT address should be different than 5)
  nearby_test_fakes_SetRandomNumber(35);
  next_time_ms =
      ADDRESS_ROTATION_PERIOD_UNWANTED_TRACKING_PROTECTION_MS + (2 * 60 * 1000);
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  fourthSpotAddress = nearby_test_fakes_GetSpotAddress();
  printf("fourthSpotAddress=0x%lx \n", fourthSpotAddress);
  ASSERT_NE(thirdSpotAddress, fourthSpotAddress);

  // 7) At 48hr+2min. SPOT address should be different than 6)
  nearby_test_fakes_SetRandomNumber(37);
  next_time_ms = ADDRESS_ROTATION_PERIOD_UNWANTED_TRACKING_PROTECTION_MS * 2 +
                 (2 * 60 * 1000);
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  fifthSpotAddress = nearby_test_fakes_GetSpotAddress();
  printf("fifthSpotAddress=0x%lx \n", fifthSpotAddress);
  ASSERT_NE(fourthSpotAddress, fifthSpotAddress);
}

// Case NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES=1 Unwanted Tracking Protection
// Mode=Off
TEST_F(SpotTest, SetAdvertisementMultipleAddressOnUnwantedTrackingOff) {
  uint64_t firstSpotAddress, secondSpotAddress, thirdSpotAddress;
  uint64_t firstBleAddress, secondBleAddress, thirdBleAddress;

  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetRandomNumber(31);
  SetEphemeralKey();
  SetOwnerKey();
  // We need to reinitialize SPOT with the new ephemeral id.
  nearby_spot_Init();
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_SPOT);

  nearby_spot_SetUnwantedTrackingProtectionMode(false);

  // 1) At 20 min. BLE and SPOT addr should be same
  nearby_test_fakes_SetRandomNumber(32);
  unsigned int next_time_ms = (20 * 60 * 1000);
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  firstSpotAddress = nearby_test_fakes_GetSpotAddress();
  firstBleAddress = nearby_platform_GetBleAddress();
  printf("firstSpotAddress=0x%lx firstBleAddress=0x%lx \n", firstSpotAddress,
         firstBleAddress);
  ASSERT_EQ(firstSpotAddress, firstBleAddress);

  // 2) At 40 min. BLE address should be different than 1. BLE and SPOT addr
  // should be same
  nearby_test_fakes_SetRandomNumber(33);
  next_time_ms = (40 * 60 * 1000);
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  secondSpotAddress = nearby_test_fakes_GetSpotAddress();
  secondBleAddress = nearby_platform_GetBleAddress();
  printf("secondSpotAddress=0x%lx secondBleAddress=0x%lx \n", secondSpotAddress,
         secondBleAddress);
  ASSERT_EQ(secondSpotAddress, secondBleAddress);
  ASSERT_NE(firstBleAddress, secondBleAddress);

  // 3) At 65 min. BLE address should be different than 2. BLE and SPOT addr
  // should be same
  nearby_test_fakes_SetRandomNumber(34);
  next_time_ms = (65 * 60 * 1000);
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  thirdSpotAddress = nearby_test_fakes_GetSpotAddress();
  thirdBleAddress = nearby_platform_GetBleAddress();
  printf("thirdSpotAddress=0x%lx thirdBleAddress=0x%lx \n", thirdSpotAddress,
         thirdBleAddress);
  ASSERT_EQ(thirdSpotAddress, thirdBleAddress);
  ASSERT_NE(secondBleAddress, thirdBleAddress);
}

#else
// Case NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES=0 Unwanted Tracking Protection
// Mode=On
TEST_F(SpotTest, SetAdvertisementMultipleAddressOffUnwantedTrackingOn) {
  uint64_t firstSpotAddress, secondSpotAddress, thirdSpotAddress,
      fourthSpotAddress;
  uint64_t firstBleAddress, secondBleAddress, thirdBleAddress, fourthBleAddress;

  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetRandomNumber(31);
  SetEphemeralKey();
  SetOwnerKey();
  // We need to reinitialize SPOT with the new ephemeral id.
  nearby_spot_Init();
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_SPOT);

  nearby_spot_SetUnwantedTrackingProtectionMode(true);
  // 1) Initial. SPOT addr should not be 0
  firstSpotAddress = nearby_test_fakes_GetSpotAddress();
  firstBleAddress = nearby_platform_GetBleAddress();
  printf("firstSpotAddress=0x%lx firstBleAddress=0x%lx \n", firstSpotAddress,
         firstBleAddress);
  ASSERT_NE(firstSpotAddress, 0);

  // 2) At 20 min. SPOT and BLE addresses should be same
  unsigned int next_time_ms = (20 * 60 * 1000);
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  secondSpotAddress = nearby_test_fakes_GetSpotAddress();
  secondBleAddress = nearby_platform_GetBleAddress();
  printf("secondSpotAddress=0x%lx secondBleAddress=0x%lx \n", secondSpotAddress,
         secondBleAddress);
  ASSERT_EQ(secondBleAddress, secondSpotAddress);

  // 3) At 3 hr. SPOT and BLE addresses should be same. BLE addr should be same
  // as 1)
  nearby_test_fakes_SetRandomNumber(34);
  next_time_ms = (3 * 60 * 60 * 1000);  // 3 hr
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  thirdBleAddress = nearby_platform_GetBleAddress();
  thirdSpotAddress = nearby_test_fakes_GetSpotAddress();
  printf("thirdSpotAddress=0x%lx thirdBleAddress=0x%lx\n", thirdSpotAddress,
         thirdBleAddress);
  ASSERT_EQ(thirdBleAddress, thirdSpotAddress);
  ASSERT_EQ(secondBleAddress, thirdBleAddress);

  // 4) At 24 hr + 2min. SPOT and BLE addresses should be same. BLE addr should
  // be different than 2)
  nearby_test_fakes_SetRandomNumber(35);
  next_time_ms =
      ADDRESS_ROTATION_PERIOD_UNWANTED_TRACKING_PROTECTION_MS + (2 * 60 * 1000);
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  fourthBleAddress = nearby_platform_GetBleAddress();
  fourthSpotAddress = nearby_test_fakes_GetSpotAddress();
  printf("fourthSpotAddress=0x%lx fourthBleAddress=0x%lx\n", fourthSpotAddress,
         fourthBleAddress);
  ASSERT_EQ(fourthBleAddress, fourthSpotAddress);
  ASSERT_NE(thirdBleAddress, fourthBleAddress);
}

// Case NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES=0 Unwanted Tracking Protection
// Mode=Off
TEST_F(SpotTest, SetAdvertisementMultipleAddressOffUnwantedTrackingOff) {
  uint64_t firstSpotAddress, secondSpotAddress, thirdSpotAddress;
  uint64_t firstBleAddress, secondBleAddress, thirdBleAddress;

  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetRandomNumber(31);
  SetEphemeralKey();
  SetOwnerKey();
  // We need to reinitialize SPOT with the new ephemeral id.
  nearby_spot_Init();
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_SPOT);
  nearby_spot_SetUnwantedTrackingProtectionMode(false);

  // 1) At 20 min. SPOT and BLE addresses should be same
  unsigned int next_time_ms = (20 * 60 * 1000);
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  firstSpotAddress = nearby_test_fakes_GetSpotAddress();
  firstBleAddress = nearby_platform_GetBleAddress();
  printf("firstSpotAddress=0x%lx firstBleAddress=0x%lx \n", firstSpotAddress,
         firstBleAddress);
  ASSERT_EQ(firstSpotAddress, firstBleAddress);

  // 2) At 40 min. SPOT and BLE addresses should be same. BLE address should be
  // different than 1)
  nearby_test_fakes_SetRandomNumber(32);
  next_time_ms = (40 * 60 * 1000);  // 40min
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  secondBleAddress = nearby_platform_GetBleAddress();
  secondSpotAddress = nearby_test_fakes_GetSpotAddress();
  printf("secondBleAddress=0x%lx secondSpotAddress=0x%lx \n", secondBleAddress,
         secondSpotAddress);
  ASSERT_EQ(secondSpotAddress, secondBleAddress);
  ASSERT_NE(firstBleAddress, secondBleAddress);

  // 3) At 65 min. SPOT and BLE addresses should be same. BLE address should be
  // different than 2)
  nearby_test_fakes_SetRandomNumber(33);
  next_time_ms = (65 * 60 * 1000);  // 65min
  nearby_test_fakes_SetCurrentTimeMs(next_time_ms);
  thirdBleAddress = nearby_platform_GetBleAddress();
  thirdSpotAddress = nearby_test_fakes_GetSpotAddress();
  printf("thirdBleAddress=0x%lx thirdSpotAddress=0x%lx \n", thirdBleAddress,
         thirdSpotAddress);
  ASSERT_EQ(thirdSpotAddress, thirdBleAddress);
  ASSERT_NE(secondBleAddress, thirdBleAddress);
}

#endif /* NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES */

#endif /* NEARBY_FP_ENABLE_SPOT */

#pragma GCC diagnostic pop

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
