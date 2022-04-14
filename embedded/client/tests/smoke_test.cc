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

#include <cerrno>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "fakes.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "nearby.h"
#include "nearby_event.h"
#include "nearby_fp_client.h"
#include "nearby_fp_library.h"
#include "nearby_platform_ble.h"
#include "nearby_platform_persistence.h"
#include "nearby_utils.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"

constexpr uint8_t kBobPrivateKey[32] = {
    0x02, 0xB4, 0x37, 0xB0, 0xED, 0xD6, 0xBB, 0xD4, 0x29, 0x06, 0x4A,
    0x4E, 0x52, 0x9F, 0xCB, 0xF1, 0xC4, 0x8D, 0x0D, 0x62, 0x49, 0x24,
    0xD5, 0x92, 0x27, 0x4B, 0x7E, 0xD8, 0x11, 0x93, 0xD7, 0x63};
constexpr uint8_t kBobPublicKey[64] = {
    0xF7, 0xD4, 0x96, 0xA6, 0x2E, 0xCA, 0x41, 0x63, 0x51, 0x54, 0x0A,
    0xA3, 0x43, 0xBC, 0x69, 0x0A, 0x61, 0x09, 0xF5, 0x51, 0x50, 0x06,
    0x66, 0xB8, 0x3B, 0x12, 0x51, 0xFB, 0x84, 0xFA, 0x28, 0x60, 0x79,
    0x5E, 0xBD, 0x63, 0xD3, 0xB8, 0x83, 0x6F, 0x44, 0xA9, 0xA3, 0xE2,
    0x8B, 0xB3, 0x40, 0x17, 0xE0, 0x15, 0xF5, 0x97, 0x93, 0x05, 0xD8,
    0x49, 0xFD, 0xF8, 0xDE, 0x10, 0x12, 0x3B, 0x61, 0xD2};

constexpr uint8_t kAlicePrivateKey[32] = {
    0xD7, 0x5E, 0x54, 0xC7, 0x7D, 0x76, 0x24, 0x89, 0xE5, 0x7C, 0xFA,
    0x92, 0x37, 0x43, 0xF1, 0x67, 0x77, 0xA4, 0x28, 0x3D, 0x99, 0x80,
    0x0B, 0xAC, 0x55, 0x58, 0x48, 0x38, 0x93, 0xE5, 0xB0, 0x6D};
constexpr uint8_t kAlicePublicKey[64] = {
    0x36, 0xAC, 0x68, 0x2C, 0x50, 0x82, 0x15, 0x66, 0x8F, 0xBE, 0xFE,
    0x24, 0x7D, 0x01, 0xD5, 0xEB, 0x96, 0xE6, 0x31, 0x8E, 0x85, 0x5B,
    0x2D, 0x64, 0xB5, 0x19, 0x5D, 0x38, 0xEE, 0x7E, 0x37, 0xBE, 0x18,
    0x38, 0xC0, 0xB9, 0x48, 0xC3, 0xF7, 0x55, 0x20, 0xE0, 0x7E, 0x70,
    0xF0, 0x72, 0x91, 0x41, 0x9A, 0xCE, 0x2D, 0x28, 0x14, 0x3C, 0x5A,
    0xDB, 0x2D, 0xBD, 0x98, 0xEE, 0x3C, 0x8E, 0x4F, 0xBF};

constexpr uint8_t kExpectedSharedSecret[32] = {
    0x9D, 0xAD, 0xE4, 0xF8, 0x6A, 0xC3, 0x48, 0x8B, 0xBA, 0xC2, 0xAC,
    0x34, 0xB5, 0xFE, 0x68, 0xA0, 0xEE, 0x5A, 0x67, 0x06, 0xF5, 0x43,
    0xD9, 0x06, 0x1A, 0xD5, 0x78, 0x89, 0x49, 0x8A, 0xE6, 0xBA};

constexpr uint8_t kExpectedAesKey[16] = {0xB0, 0x7F, 0x1F, 0x17, 0xC2, 0x36,
                                         0xCB, 0xD3, 0x35, 0x23, 0xC5, 0x15,
                                         0xF3, 0x50, 0xAE, 0x57};

constexpr uint64_t kRemoteDevice = 0xB0B1B2B3B4B5;

constexpr uint8_t kTxPower = 33;
constexpr uint8_t kDiscoverableAdvertisement[] = {
    6, 0x16, 0x2C, 0xFE, 0x10, 0x11, 0x12, 2, 0x0A, kTxPower};

constexpr uint8_t kSeekerAccountKey[16] = {0x04, 20, 21, 22, 23, 24, 25, 26,
                                           27,   28, 29, 30, 31, 32, 33, 34};
constexpr uint8_t kSeekerAccountKey2[16] = {0x04, 50, 51, 52, 53, 54, 55, 56,
                                            57,   58, 59, 60, 61, 62, 63, 64};

using ::testing::ElementsAreArray;

static std::string VecToString(std::vector<uint8_t> data) {
  std::stringstream output;
  output << "0x" << std::hex;
  for (int i = 0; i < data.size(); i++) {
    output << std::setfill('0') << std::setw(2) << (unsigned)data[i];
  }
  return output.str();
}

// static std::string VecToString(uint8_t* start, uint8_t* end) {
//   return VecToString(std::vector<uint8_t>(start, end));
// }

class Event {
 public:
  explicit Event(nearby_event_Type type) : type_(type) {}
  explicit Event(const nearby_event_Event* event) : Event(event->event_type) {}

  nearby_event_Type GetType() const { return type_; }

  virtual bool operator==(const Event& b) const { return type_ == b.type_; }

  virtual ~Event() {}

 protected:
  virtual std::string ToString() const {
    std::stringstream output;
    output << "Event type: " << type_;
    return output.str();
  }
  nearby_event_Type type_;
  friend std::ostream& operator<<(std::ostream& os, const Event& event);
};

class MessageStreamConnectedEvent : public Event {
 public:
  explicit MessageStreamConnectedEvent(uint64_t peer_address)
      : Event(kNearbyEventMessageStreamConnected),
        peer_address_(peer_address) {}
  explicit MessageStreamConnectedEvent(
      const nearby_event_MessageStreamConnected* payload)
      : Event(kNearbyEventMessageStreamConnected),
        peer_address_(payload->peer_address) {
    EXPECT_NE(nullptr, payload);
  }
  explicit MessageStreamConnectedEvent(const nearby_event_Event* event)
      : MessageStreamConnectedEvent(
            (const nearby_event_MessageStreamConnected*)event->payload) {
    EXPECT_EQ(kNearbyEventMessageStreamConnected, type_);
  }

  virtual bool operator==(const Event& b) const override {
    if (type_ != b.GetType()) return false;
    const MessageStreamConnectedEvent* event =
        (const MessageStreamConnectedEvent*)&b;
    return peer_address_ == event->peer_address_;
  }

  std::string ToString() const override {
    std::stringstream output;
    output << "Event type: " << type_ << " peer_address: " << peer_address_;
    return output.str();
  }

 private:
  uint64_t peer_address_;
};

class MessageStreamDisconnectedEvent : public Event {
 public:
  explicit MessageStreamDisconnectedEvent(uint64_t peer_address)
      : Event(kNearbyEventMessageStreamDisconnected),
        peer_address_(peer_address) {}
  explicit MessageStreamDisconnectedEvent(
      const nearby_event_MessageStreamDisconnected* payload)
      : Event(kNearbyEventMessageStreamDisconnected),
        peer_address_(payload->peer_address) {
    EXPECT_NE(nullptr, payload);
  }
  explicit MessageStreamDisconnectedEvent(const nearby_event_Event* event)
      : MessageStreamDisconnectedEvent(
            (const nearby_event_MessageStreamDisconnected*)event->payload) {
    EXPECT_EQ(kNearbyEventMessageStreamDisconnected, type_);
  }

  virtual bool operator==(const Event& b) const override {
    if (type_ != b.GetType()) return false;
    const MessageStreamDisconnectedEvent* event =
        (const MessageStreamDisconnectedEvent*)&b;
    return peer_address_ == event->peer_address_;
  }

  std::string ToString() const override {
    std::stringstream output;
    output << "Event type: " << type_ << " peer_address: " << peer_address_;
    return output.str();
  }

 private:
  uint64_t peer_address_;
};

class MessageStreamReceivedEvent : public Event {
 public:
  explicit MessageStreamReceivedEvent(
      const nearby_event_MessageStreamReceived* payload)
      : Event(kNearbyEventMessageStreamReceived) {
    EXPECT_NE(nullptr, payload);

    peer_address_ = payload->peer_address;
    group_ = payload->message_group;
    code_ = payload->message_code;
    if (payload->length > 0) {
      data_ =
          std::vector<uint8_t>(payload->data, payload->data + payload->length);
    }
  }
  explicit MessageStreamReceivedEvent(const nearby_event_Event* event)
      : MessageStreamReceivedEvent(
            (const nearby_event_MessageStreamReceived*)event->payload) {
    EXPECT_EQ(kNearbyEventMessageStreamReceived, event->event_type);
  }

  virtual bool operator==(const Event& b) const override {
    if (type_ != b.GetType()) return false;
    const MessageStreamReceivedEvent* event =
        (const MessageStreamReceivedEvent*)&b;
    return peer_address_ == event->peer_address_ && group_ == event->group_ &&
           code_ == event->code_ && data_ == event->data_;
  }

  std::string ToString() const override {
    std::stringstream output;
    output << "Event type: " << type_ << " peer_address: " << peer_address_
           << " group: " << (int)group_ << " code: " << (int)code_
           << " length: " << data_.size();
    if (data_.size() > 0) {
      output << " data: " << VecToString(data_);
    }
    return output.str();
  }

 private:
  uint64_t peer_address_;
  uint8_t group_;
  uint8_t code_;
  std::vector<uint8_t> data_;
};

std::ostream& operator<<(std::ostream& os, const Event& event) {
  os << event.ToString();
  return os;
}

static std::unique_ptr<Event> GetEvent(const nearby_event_Event* event) {
  switch (event->event_type) {
    case kNearbyEventMessageStreamConnected:
      return std::make_unique<MessageStreamConnectedEvent>(event);
    case kNearbyEventMessageStreamDisconnected:
      return std::make_unique<MessageStreamDisconnectedEvent>(event);
    case kNearbyEventMessageStreamReceived:
      return std::make_unique<MessageStreamReceivedEvent>(event);
  }
  return std::make_unique<Event>(event);
}

std::vector<std::unique_ptr<Event>> message_stream_events;
static void OnEventCallback(nearby_event_Event* event) {
  message_stream_events.push_back(GetEvent(event));
}

constexpr nearby_fp_client_Callbacks kClientCallbacks = {.on_event =
                                                             OnEventCallback};

static void WriteToAccountKey() {
  uint8_t encrypted_account_key_write_request[16];
  nearby_test_fakes_Aes128Encrypt(
      kSeekerAccountKey, encrypted_account_key_write_request, kExpectedAesKey);
  nearby_fp_fakes_ReceiveAccountKeyWrite(
      encrypted_account_key_write_request,
      sizeof(encrypted_account_key_write_request));
}

int GetCapability(uint64_t peer_address) {
  nearby_fp_client_SeekerInfo seeker_infos[NEARBY_MAX_RFCOMM_CONNECTIONS];
  size_t sl = NEARBY_MAX_RFCOMM_CONNECTIONS;
  nearby_fp_client_GetSeekerInfo(seeker_infos, &sl);

  for (int i = 0; i < sl; i++) {
    if (seeker_infos[i].peer_address == peer_address) {
      return seeker_infos[i].capabilities;
    }
  }
  return -1;
}

// |flags| from Table 1.2.1: Raw Request (type 0x00)  in FP specification
static void Pair(uint8_t flags) {
  uint8_t salt = 0xAB;

  nearby_test_fakes_SetAntiSpoofingKey(kBobPrivateKey, kBobPublicKey);
  nearby_test_fakes_SetRandomNumber(salt);
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_DISCOVERABLE);

  uint8_t request[16];
  request[0] = 0x00;  // key-based pairing request
  request[1] = flags;
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // Seeker's address
  request[8] = 0xB0;
  request[9] = 0xB1;
  request[10] = 0xB2;
  request[11] = 0xB3;
  request[12] = 0xB4;
  request[13] = 0xB5;
  // salt
  request[14] = 0xCD;
  request[15] = 0xEF;
  uint8_t encrypted[16 + 64];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);
  // Seeker responds
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));
  // BT negotatiates passkey 123456 (0x01E240)
  uint8_t raw_passkey_block[16] = {0x02, 0x01, 0xE2, 0x40};
  uint8_t encrypted_passkey_block[16];
  nearby_test_fakes_Aes128Encrypt(raw_passkey_block, encrypted_passkey_block,
                                  kExpectedAesKey);
  // Seeker sends the passkey to provider
  nearby_fp_fakes_ReceivePasskey(encrypted_passkey_block,
                                 sizeof(encrypted_passkey_block));

  // Seeker sends their account key
  WriteToAccountKey();
}

TEST(NearbyFpClient, Init) {
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_Init(NULL));
}

TEST(NearbyFpClient, AccountKeyListIsEmpty) {
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_Init(NULL));
  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(0, keys.size());
}

TEST(NearbyFpClient, CopyBigEndian_4bytes) {
  uint8_t buffer[4];
  uint32_t value = 0x01020304;

  nearby_utils_CopyBigEndian(buffer, value, 4);

  ASSERT_EQ(1, buffer[0]);
  ASSERT_EQ(2, buffer[1]);
  ASSERT_EQ(3, buffer[2]);
  ASSERT_EQ(4, buffer[3]);
}

TEST(NearbyFpClient, CopyBigEndian_3bytes) {
  uint8_t buffer[3];
  uint32_t value = 0x00010203;

  nearby_utils_CopyBigEndian(buffer, value, 3);

  ASSERT_EQ(1, buffer[0]);
  ASSERT_EQ(2, buffer[1]);
  ASSERT_EQ(3, buffer[2]);
}

TEST(NearbyFpClient, AdvertisementDiscoverable) {
  const int kBufferSize = DISCOVERABLE_ADV_SIZE_BYTES;
  uint8_t buffer[kBufferSize];
  nearby_fp_client_Init(NULL);

  size_t written =
      nearby_fp_CreateDiscoverableAdvertisement(buffer, kBufferSize);
  written += nearby_fp_AppendTxPower(buffer + written, kBufferSize - written,
                                     kTxPower);

  ASSERT_EQ(kBufferSize, written);
  ASSERT_THAT(std::vector<uint8_t>(buffer, buffer + kBufferSize),
              ElementsAreArray(kDiscoverableAdvertisement));
}

TEST(NearbyFpClient, AdvertisementNondiscoverable_noKeys) {
  const int kBufferSize = 9;
  uint8_t buffer[kBufferSize];
  const uint8_t kExpectedResult[] = {5,    0x16, 0x2C, 0xFE,    0x00,
                                     0x00, 2,    0x0A, kTxPower};
  nearby_fp_client_Init(NULL);

  size_t written =
      nearby_fp_CreateNondiscoverableAdvertisement(buffer, kBufferSize, false);
  written += nearby_fp_AppendTxPower(buffer + written, kBufferSize - written,
                                     kTxPower);

  ASSERT_EQ(kBufferSize, written);
  ASSERT_THAT(std::vector<uint8_t>(buffer, buffer + kBufferSize),
              ElementsAreArray(kExpectedResult));
}

TEST(NearbyFpClient, AdvertisementNondiscoverable_oneKey) {
  const int kBufferSize = 15;
  uint8_t buffer[kBufferSize];
  uint8_t salt = 0xC7;
  uint8_t account_keys[] = {1,    0x11, 0x22, 0x33, 0x44, 0x55,
                            0x66, 0x77, 0x88, 0x99, 0x00, 0xAA,
                            0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  const uint8_t kExpectedResult[] = {11,   0x16, 0x2C, 0xFE, 0x00,
                                     0x42, 0x0A, 0x42, 0x88, 0x10,
                                     0x11, salt, 2,    0x0A, kTxPower};
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetAccountKeys(account_keys, sizeof(account_keys));
  nearby_test_fakes_SetRandomNumber(salt);
  nearby_fp_LoadAccountKeys();

  size_t written =
      nearby_fp_CreateNondiscoverableAdvertisement(buffer, kBufferSize, false);
  written += nearby_fp_AppendTxPower(buffer + written, kBufferSize - written,
                                     kTxPower);

  ASSERT_EQ(kBufferSize, written);
  ASSERT_THAT(std::vector<uint8_t>(buffer, buffer + kBufferSize),
              ElementsAreArray(kExpectedResult));
}

TEST(NearbyFpClient, AdvertisementNondiscoverable_twoKeys) {
  const int kBufferSize = 16;
  uint8_t buffer[kBufferSize];
  uint8_t salt = 0xC7;
  uint8_t account_keys[] = {
      2,    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
      0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x11, 0x22, 0x22, 0x33,
      0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88};
  const uint8_t kExpectedResult[] = {12,   0x16, 0x2C, 0xFE,    0x00, 0x52,
                                     0x2F, 0xBA, 0x06, 0x42,    0x00, 0x11,
                                     salt, 2,    0x0A, kTxPower};
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetAccountKeys(account_keys, sizeof(account_keys));
  nearby_test_fakes_SetRandomNumber(salt);
  nearby_fp_LoadAccountKeys();

  size_t written =
      nearby_fp_CreateNondiscoverableAdvertisement(buffer, kBufferSize, false);
  written += nearby_fp_AppendTxPower(buffer + written, kBufferSize - written,
                                     kTxPower);

  ASSERT_EQ(kBufferSize, written);
  ASSERT_THAT(std::vector<uint8_t>(buffer, buffer + kBufferSize),
              ElementsAreArray(kExpectedResult));
}

TEST(NearbyFpClient, GattReadModelId) {
  uint8_t buffer[3];
  size_t length = sizeof(buffer);
  nearby_fp_client_Init(NULL);

  ASSERT_EQ(kNearbyStatusOK,
            nearby_test_fakes_GattReadModelId(buffer, &length));

  ASSERT_EQ(sizeof(buffer), length);
  ASSERT_EQ(0x10, buffer[0]);
  ASSERT_EQ(0x11, buffer[1]);
  ASSERT_EQ(0x12, buffer[2]);
}

TEST(NearbyFpClient, SetAntiSpoofingKey) {
  nearby_fp_client_Init(NULL);

  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kBobPrivateKey, kBobPublicKey));
}

TEST(NearbyFpClient, GenSec256r1Secret_bobAlice) {
  uint8_t secret[32];
  nearby_fp_client_Init(NULL);

  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kBobPrivateKey, kBobPublicKey));
  ASSERT_EQ(kNearbyStatusOK,
            nearby_test_fakes_GenSec256r1Secret(kAlicePublicKey, secret));

  for (int i = 0; i < sizeof(secret); i++) {
    ASSERT_EQ(kExpectedSharedSecret[i], secret[i])
        << "Difference at position: " << i;
  }
}

TEST(NearbyFpClient, GenSec256r1Secret_aliceBob) {
  uint8_t secret[32];
  nearby_fp_client_Init(NULL);

  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kAlicePrivateKey, kAlicePublicKey));
  ASSERT_EQ(kNearbyStatusOK,
            nearby_test_fakes_GenSec256r1Secret(kBobPublicKey, secret));

  for (int i = 0; i < sizeof(secret); i++) {
    ASSERT_EQ(kExpectedSharedSecret[i], secret[i])
        << "Difference at position: " << i;
  }
}

TEST(NearbyFpClient, CreateSharedSecret_aliceBob) {
  uint8_t secret[16];
  nearby_fp_client_Init(NULL);
  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kAlicePrivateKey, kAlicePublicKey));

  ASSERT_EQ(kNearbyStatusOK,
            nearby_fp_CreateSharedSecret(kBobPublicKey, secret));

  for (int i = 0; i < sizeof(secret); i++) {
    ASSERT_EQ(kExpectedAesKey[i], secret[i]) << "Difference at position: " << i;
  }
}

TEST(NearbyFpClient, CreateSharedSecret_bobAlice) {
  uint8_t secret[16];
  nearby_fp_client_Init(NULL);
  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kBobPrivateKey, kBobPublicKey));

  ASSERT_EQ(kNearbyStatusOK,
            nearby_fp_CreateSharedSecret(kAlicePublicKey, secret));

  for (int i = 0; i < sizeof(secret); i++) {
    ASSERT_EQ(kExpectedAesKey[i], secret[i]) << "Difference at position: " << i;
  }
}

#ifdef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
TEST(NearbyFpClient,
     SetAdvertisementWithBatteryNotification_AdvertisementWithPairingUI) {
  uint8_t salt = 0xC7;
  uint8_t account_keys[] = {
      5,    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA,
      0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44,
      0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88, 0x03, 0x13, 0x23,
      0x33, 0x43, 0x53, 0x63, 0x73, 0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3,
      0xF3, 0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 0x84, 0x94, 0xA4,
      0xB4, 0xC4, 0xD4, 0xE4, 0xF4, 0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65,
      0x75, 0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5,
  };
  const uint8_t kExpectedResult[] = {
      0x10, 0x16, 0x2c, 0xfe, 0x00, 0x90, 0x03, 0x78, 0x95, 0x67,
      0x0c, 0xc3, 0x0a, 0xcc, 0x56, 0x11, salt, 0x02, 0x0a, kTxPower};
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetAntiSpoofingKey(kBobPrivateKey, kBobPublicKey);
  nearby_test_fakes_SetAccountKeys(account_keys, sizeof(account_keys));
  nearby_test_fakes_SetRandomNumber(salt);
  nearby_fp_LoadAccountKeys();

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE |
                                 NEARBY_FP_ADVERTISEMENT_PAIRING_UI_INDICATOR));

  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kExpectedResult));
}

TEST(
    NearbyFpClient,
    SetAdvertisementWithBatteryNotification_AdvertisementNoPairingUIWithBatteryUI) {
  uint8_t salt = 0xC7;
  uint8_t account_keys[] = {
      5,    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA,
      0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44,
      0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88, 0x03, 0x13, 0x23,
      0x33, 0x43, 0x53, 0x63, 0x73, 0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3,
      0xF3, 0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 0x84, 0x94, 0xA4,
      0xB4, 0xC4, 0xD4, 0xE4, 0xF4, 0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65,
      0x75, 0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5,
  };
  const uint8_t kExpectedResult[] = {0x14, 0x16, 0x2c, 0xfe, 0x00, 0x92,
                                     0x30, 0xa6, 0x17, 0x10, 0x0c, 0x6c,
                                     0xa9, 0xea, 0xf7, 0x11, salt, 0x33,
                                     0xd5, 0xd0, 0xda, 2,    0x0a, kTxPower};
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetAntiSpoofingKey(kBobPrivateKey, kBobPublicKey);
  nearby_test_fakes_SetAccountKeys(account_keys, sizeof(account_keys));
  nearby_test_fakes_SetRandomNumber(salt);
  nearby_fp_LoadAccountKeys();

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE |
                                 NEARBY_FP_ADVERTISEMENT_BATTERY_UI_INDICATOR |
                                 NEARBY_FP_ADVERTISEMENT_INCLUDE_BATTERY_INFO));

  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kExpectedResult));
}

TEST(
    NearbyFpClient,
    SetAdvertisementWithBatteryNotification_Charging_AdvertisementContainsBatteryInfo) {
  uint8_t salt = 0xC7;
  uint8_t account_keys[] = {
      5,    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA,
      0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44,
      0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88, 0x03, 0x13, 0x23,
      0x33, 0x43, 0x53, 0x63, 0x73, 0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3,
      0xF3, 0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 0x84, 0x94, 0xA4,
      0xB4, 0xC4, 0xD4, 0xE4, 0xF4, 0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65,
      0x75, 0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5,
  };
  const uint8_t kExpectedResult[] = {0x14, 0x16, 0x2c, 0xfe, 0x00, 0x90,
                                     0x30, 0xa6, 0x17, 0x10, 0x0c, 0x6c,
                                     0xa9, 0xea, 0xf7, 0x11, salt, 0x33,
                                     0xd5, 0xd0, 0xda, 2,    0x0a, kTxPower};
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetAntiSpoofingKey(kBobPrivateKey, kBobPublicKey);
  nearby_test_fakes_SetAccountKeys(account_keys, sizeof(account_keys));
  nearby_test_fakes_SetRandomNumber(salt);
  nearby_fp_LoadAccountKeys();
  nearby_test_fakes_SetIsCharging(true);

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE |
                                 NEARBY_FP_ADVERTISEMENT_PAIRING_UI_INDICATOR |
                                 NEARBY_FP_ADVERTISEMENT_BATTERY_UI_INDICATOR |
                                 NEARBY_FP_ADVERTISEMENT_INCLUDE_BATTERY_INFO));

  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kExpectedResult));
}

TEST(
    NearbyFpClient,
    SetAdvertisementWithBatteryNotification_NotCharging_AdvertisementContainsBatteryInfo) {
  uint8_t salt = 0xC7;
  uint8_t account_keys[] = {
      5,    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA,
      0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44,
      0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88, 0x03, 0x13, 0x23,
      0x33, 0x43, 0x53, 0x63, 0x73, 0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3,
      0xF3, 0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 0x84, 0x94, 0xA4,
      0xB4, 0xC4, 0xD4, 0xE4, 0xF4, 0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65,
      0x75, 0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5,
  };
  const uint8_t kExpectedResult[] = {0x14, 0x16, 0x2c, 0xfe, 0x00, 0x90,
                                     0x46, 0x84, 0x1e, 0x84, 0x2e, 0x27,
                                     0x05, 0x92, 0xcc, 0x11, salt, 0x33,
                                     0x55, 0x50, 0x5a, 2,    0x0a, kTxPower};
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetAntiSpoofingKey(kBobPrivateKey, kBobPublicKey);
  nearby_test_fakes_SetAccountKeys(account_keys, sizeof(account_keys));
  nearby_test_fakes_SetRandomNumber(salt);
  nearby_fp_LoadAccountKeys();
  nearby_test_fakes_SetIsCharging(false);

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE |
                                 NEARBY_FP_ADVERTISEMENT_PAIRING_UI_INDICATOR |
                                 NEARBY_FP_ADVERTISEMENT_BATTERY_UI_INDICATOR |
                                 NEARBY_FP_ADVERTISEMENT_INCLUDE_BATTERY_INFO));

  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kExpectedResult));
}

TEST(
    NearbyFpClient,
    SetAdvertisementWithBatteryNotification_GetBatteryInfoFails_AdvertismentIsValid) {
  uint8_t salt = 0xC7;
  uint8_t account_keys[] = {
      5,    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA,
      0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44,
      0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88, 0x03, 0x13, 0x23,
      0x33, 0x43, 0x53, 0x63, 0x73, 0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3,
      0xF3, 0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 0x84, 0x94, 0xA4,
      0xB4, 0xC4, 0xD4, 0xE4, 0xF4, 0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65,
      0x75, 0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5,
  };
  const uint8_t kExpectedResult[] = {
      0x10, 0x16, 0x2c, 0xfe, 0x00, 0x90, 0x03, 0x78, 0x95, 0x67,
      0x0c, 0xc3, 0x0a, 0xcc, 0x56, 0x11, salt, 2,    0x0a, kTxPower};
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetAntiSpoofingKey(kBobPrivateKey, kBobPublicKey);
  nearby_test_fakes_SetAccountKeys(account_keys, sizeof(account_keys));
  nearby_test_fakes_SetRandomNumber(salt);
  nearby_fp_LoadAccountKeys();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnsupported);

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE |
                                 NEARBY_FP_ADVERTISEMENT_PAIRING_UI_INDICATOR |
                                 NEARBY_FP_ADVERTISEMENT_BATTERY_UI_INDICATOR |
                                 NEARBY_FP_ADVERTISEMENT_INCLUDE_BATTERY_INFO));

  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kExpectedResult));
}

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient,
     RfcommConnected_HasBatteryInfo_SendsModelIdBleAddressAndBatteryInfo) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kExpectedRfcommOutput[] = {// Model Id
                                               3, 1, 0, 3, 0x10, 0x11, 0x12,
                                               // Ble Address
                                               3, 2, 0, 6, 0x6b, 0xab, 0xab,
                                               0xab, 0xab, 0xab,
                                               // Battery level
                                               3, 3, 0, 3, 0xd5, 0xd0, 0xda,
                                               // Battery remaining time
                                               3, 4, 0, 1, 100};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_BatteryTime(100);
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetIsCharging(true);

  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusOK);
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  ASSERT_EQ(1, message_stream_events.size());
  ASSERT_EQ(MessageStreamConnectedEvent(kPeerAddress),
            *message_stream_events[0]);
  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}
#endif /* NEARBY_FP_MESSAGE_STREAM */
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient, EnableSilenceMode_RfcommConnected) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kExpectedRfcommOutput[] = {// Model Id
                                               3, 1, 0, 3, 0x10, 0x11, 0x12,
                                               // Ble Address
                                               3, 2, 0, 6, 0x6b, 0xab, 0xab,
                                               0xab, 0xab, 0xab,
                                               // Battery level
                                               3, 3, 0, 3, 0xd5, 0xd0, 0xda,
                                               // Battery remaining time
                                               3, 4, 0, 1, 100,
                                               // Enable silence mode
                                               1, 1, 0, 0};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_BatteryTime(100);
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_fp_client_SetSilenceMode(kPeerAddress, true);

  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient, DisableSilenceMode_RfcommConnected) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kExpectedRfcommOutput[] = {// Model Id
                                               3, 1, 0, 3, 0x10, 0x11, 0x12,
                                               // Ble Address
                                               3, 2, 0, 6, 0x6b, 0xab, 0xab,
                                               0xab, 0xab, 0xab,
                                               // Battery level
                                               3, 3, 0, 3, 0xd5, 0xd0, 0xda,
                                               // Battery remaining time
                                               3, 4, 0, 1, 100,
                                               // Disable silence mode
                                               1, 2, 0, 0};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_BatteryTime(100);
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_fp_client_SetSilenceMode(kPeerAddress, false);

  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient, BatteryLevelLongForm_RfcommConnected) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kExpectedRfcommOutput[] = {// Model Id
                                               3, 1, 0, 3, 0x10, 0x11, 0x12,
                                               // Ble Address
                                               3, 2, 0, 6, 0x6b, 0xab, 0xab,
                                               0xab, 0xab, 0xab,
                                               // Battery level
                                               3, 3, 0, 3, 0xd5, 0xd0, 0xda,
                                               // Battery remaining time (256)
                                               3, 4, 0, 2, 1, 0,
                                               // Disable silence mode
                                               1, 2, 0, 0};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_BatteryTime(0x100);
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_fp_client_SetSilenceMode(kPeerAddress, false);

  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient, EnableSilenceMode_NoRfcommConnection_ReturnsError) {
  constexpr uint64_t kPeerAddress = 0x123456;
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();

  ASSERT_EQ(kNearbyStatusError,
            nearby_fp_client_SetSilenceMode(kPeerAddress, true));
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient, SignalLogBufferFull_RfcommConnected) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kExpectedRfcommOutput[] = {// Model Id
                                               3, 1, 0, 3, 0x10, 0x11, 0x12,
                                               // Ble Address
                                               3, 2, 0, 6, 0x6b, 0xab, 0xab,
                                               0xab, 0xab, 0xab,
                                               // Battery level
                                               3, 3, 0, 3, 0xd5, 0xd0, 0xda,
                                               // Battery remaining time
                                               3, 4, 0, 1, 100,
                                               // Signal log buffer full
                                               2, 1, 0, 0};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_BatteryTime(100);
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_fp_client_SignalLogBufferFull(kPeerAddress);

  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient,
     ReceiveActiveComponentsRequest_SendsActiveComponentResponse) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kPeerMessage[] = {// active components request
                                      3, 5, 0, 0};
  const nearby_event_MessageStreamReceived kExpectedMessage = {
      .peer_address = kPeerAddress,
      .message_group = kPeerMessage[0],
      .message_code = kPeerMessage[1],
      .length = kPeerMessage[2] * 256 + kPeerMessage[3],
      .data = NULL};
  constexpr uint8_t kExpectedRfcommOutput[] = {// Model Id
                                               3, 1, 0, 3, 0x10, 0x11, 0x12,
                                               // Ble Address
                                               3, 2, 0, 6, 0x6b, 0xab, 0xab,
                                               0xab, 0xab, 0xab,
                                               // Battery level
                                               3, 3, 0, 3, 0xd5, 0xd0, 0xda,
                                               // Battery remaining time
                                               3, 4, 0, 1, 100,
                                               // Active component response
                                               3, 6, 0, 1, 0x00};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_BatteryTime(100);
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_test_fakes_MessageStreamReceived(kPeerAddress, kPeerMessage,
                                          sizeof(kPeerMessage));

  ASSERT_EQ(MessageStreamReceivedEvent(&kExpectedMessage),
            *message_stream_events[1]);

  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient, ReceiveCapabilities) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kCapabilities = 0x11;
  constexpr uint8_t kPeerMessage[] = {// Seeker capabilities request,
                                      // Companion app, silence mode.
                                      3, 7, 0, 1, kCapabilities};
  const nearby_event_MessageStreamReceived kExpectedMessage = {
      .peer_address = kPeerAddress,
      .message_group = kPeerMessage[0],
      .message_code = kPeerMessage[1],
      .length = kPeerMessage[2] * 256 + kPeerMessage[3],
      .data = (uint8_t*)kPeerMessage + 4};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_test_fakes_MessageStreamReceived(kPeerAddress, kPeerMessage,
                                          sizeof(kPeerMessage));

  ASSERT_EQ(MessageStreamReceivedEvent(&kExpectedMessage),
            *message_stream_events[1]);
  ASSERT_EQ(kCapabilities, GetCapability(kPeerAddress));
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient, ReceivePlatformType) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kPeerMessage[] = {// platform type request,
                                      // Android, Pie SDK
                                      3, 8, 0, 2, 0x01, 0x1c};
  const nearby_event_MessageStreamReceived kExpectedMessage = {
      .peer_address = kPeerAddress,
      .message_group = kPeerMessage[0],
      .message_code = kPeerMessage[1],
      .length = kPeerMessage[2] * 256 + kPeerMessage[3],
      .data = (uint8_t*)kPeerMessage + 4};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_test_fakes_MessageStreamReceived(kPeerAddress, kPeerMessage,
                                          sizeof(kPeerMessage));

  ASSERT_EQ(MessageStreamReceivedEvent(&kExpectedMessage),
            *message_stream_events[1]);
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient, ReceiveRingRequest) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kRingTimeSeconds = 100;
  constexpr uint16_t kRingTimeDeciseconds = 10 * kRingTimeSeconds;
  constexpr uint8_t kPeerMessage[] = {
      // Ring request, both buds,
      // 100 seconds.
      4,
      1,
      0,
      2,
      MESSAGE_CODE_RING_LEFT | MESSAGE_CODE_RING_RIGHT,
      kRingTimeSeconds};
  const nearby_event_MessageStreamReceived kExpectedMessage = {
      .peer_address = kPeerAddress,
      .message_group = kPeerMessage[0],
      .message_code = kPeerMessage[1],
      .length = kPeerMessage[2] * 256 + kPeerMessage[3],
      .data = (uint8_t*)kPeerMessage + 4};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_test_fakes_MessageStreamReceived(kPeerAddress, kPeerMessage,
                                          sizeof(kPeerMessage));

  ASSERT_EQ(MessageStreamReceivedEvent(&kExpectedMessage),
            *message_stream_events[1]);
  ASSERT_EQ(nearby_test_fakes_GetRingCommand(),
            MESSAGE_CODE_RING_LEFT | MESSAGE_CODE_RING_RIGHT);
  ASSERT_EQ(nearby_test_fakes_GetRingTimeout(), kRingTimeDeciseconds);
}
#endif /* NEARBY_FP_MESSAGE_STREAM */

TEST(NearbyFpClient, Pairing_ProviderInitiated) {
  uint8_t salt = 0xAB;

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_Init(NULL));
  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kBobPrivateKey, kBobPublicKey));
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_DISCOVERABLE));
  nearby_test_fakes_SetRandomNumber(salt);

  // Provider sets the advertisement
  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kDiscoverableAdvertisement));

  uint8_t request[16];
  request[0] = 0x00;  // key-based pairing request
  request[1] = 0x40;  // bit 1 (msb) set
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // Seeker's address
  request[8] = 0xB0;
  request[9] = 0xB1;
  request[10] = 0xB2;
  request[11] = 0xB3;
  request[12] = 0xB4;
  request[13] = 0xB5;
  // salt
  request[14] = 0xCD;
  request[15] = 0xEF;
  uint8_t encrypted[16 + 64];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);
  // Seeker responds
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  auto response = nearby_test_fakes_GetGattNotifications().at(kKeyBasedPairing);
  std::cout << VecToString(response) << std::endl;
  uint8_t decrypted_response[16];
  uint8_t expected_decrypted_response[16] = {0x01, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
                                             0xA5, salt, salt, salt, salt, salt,
                                             salt, salt, salt, salt};
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_decrypted_response[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  // Provider sends pairing request
  ASSERT_EQ(kRemoteDevice, nearby_test_fakes_GetPairingRequestAddress());

  // BT negotatiates passkey 123456 (0x01E240)
  uint8_t raw_passkey_block[16] = {0x02, 0x01, 0xE2, 0x40};
  uint8_t encrypted_passkey_block[16];
  nearby_test_fakes_Aes128Encrypt(raw_passkey_block, encrypted_passkey_block,
                                  kExpectedAesKey);
  // Seeker sends the passkey to provider
  nearby_fp_fakes_ReceivePasskey(encrypted_passkey_block,
                                 sizeof(encrypted_passkey_block));

  // Provider sends the passkey to seeker
  response = nearby_test_fakes_GetGattNotifications().at(kPasskey);
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  uint8_t expected_passkey_block[16] = {0x03, 0x01, 0xE2, 0x40, salt, salt,
                                        salt, salt, salt, salt, salt, salt,
                                        salt, salt, salt, salt};
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_passkey_block[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  ASSERT_EQ(123456, nearby_test_fakes_GetRemotePasskey());
  ASSERT_EQ(kRemoteDevice, nearby_test_fakes_GetPairedDevice());

  // Seeker sends their account key
  WriteToAccountKey();

  std::cout << "Account keys: "
            << VecToString(nearby_test_fakes_GetRawAccountKeys()) << std::endl;
  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(1, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);
}

// Pairing flow where the Seeker writes to the account key a little bit too
// early - while the BT bonding is still in progress
TEST(NearbyFpClient, Pairing_WriteKeyBeforePaired_PairingSuccessful) {
  uint8_t salt = 0xAB;

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_Init(NULL));
  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kBobPrivateKey, kBobPublicKey));
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_DISCOVERABLE));
  nearby_test_fakes_SetRandomNumber(salt);

  // Provider sets the advertisement
  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kDiscoverableAdvertisement));

  uint8_t request[16];
  request[0] = 0x00;  // key-based pairing request
  request[1] = 0x40;  // bit 1 (msb) set
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // Seeker's address
  request[8] = 0xB0;
  request[9] = 0xB1;
  request[10] = 0xB2;
  request[11] = 0xB3;
  request[12] = 0xB4;
  request[13] = 0xB5;
  // salt
  request[14] = 0xCD;
  request[15] = 0xEF;
  uint8_t encrypted[16 + 64];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);
  // Seeker responds
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  auto response = nearby_test_fakes_GetGattNotifications().at(kKeyBasedPairing);
  std::cout << VecToString(response) << std::endl;
  uint8_t decrypted_response[16];
  uint8_t expected_decrypted_response[16] = {0x01, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
                                             0xA5, salt, salt, salt, salt, salt,
                                             salt, salt, salt, salt};
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_decrypted_response[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  // Provider sends pairing request
  ASSERT_EQ(kRemoteDevice, nearby_test_fakes_GetPairingRequestAddress());

  // Seeker sends their account key
  WriteToAccountKey();

  // BT negotatiates passkey 123456 (0x01E240)
  uint8_t raw_passkey_block[16] = {0x02, 0x01, 0xE2, 0x40};
  uint8_t encrypted_passkey_block[16];
  nearby_test_fakes_Aes128Encrypt(raw_passkey_block, encrypted_passkey_block,
                                  kExpectedAesKey);
  // Seeker sends the passkey to provider
  nearby_fp_fakes_ReceivePasskey(encrypted_passkey_block,
                                 sizeof(encrypted_passkey_block));

  // Provider sends the passkey to seeker
  response = nearby_test_fakes_GetGattNotifications().at(kPasskey);
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  uint8_t expected_passkey_block[16] = {0x03, 0x01, 0xE2, 0x40, salt, salt,
                                        salt, salt, salt, salt, salt, salt,
                                        salt, salt, salt, salt};
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_passkey_block[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  ASSERT_EQ(123456, nearby_test_fakes_GetRemotePasskey());
  ASSERT_EQ(kRemoteDevice, nearby_test_fakes_GetPairedDevice());

  std::cout << "Account keys: "
            << VecToString(nearby_test_fakes_GetRawAccountKeys()) << std::endl;
  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(1, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);
}

TEST(NearbyFpClient, Pairing_SeekerInitiated_PairingSuccessful) {
  uint8_t salt = 0xAB;

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_Init(NULL));
  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kBobPrivateKey, kBobPublicKey));
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_DISCOVERABLE));
  nearby_test_fakes_SetRandomNumber(salt);

  // Provider sets the advertisement
  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kDiscoverableAdvertisement));

  uint8_t request[16];
  request[0] = 0x00;  // key-based pairing request
  request[1] = 0x00;  // bit 1 (msb) cleared
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // salt
  for (int i = 8; i < 16; i++) {
    request[i] = 0xCD + i;
  }
  uint8_t encrypted[16 + 64];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);
  // Seeker responds
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  auto response = nearby_test_fakes_GetGattNotifications().at(kKeyBasedPairing);
  std::cout << VecToString(response) << std::endl;
  uint8_t decrypted_response[16];
  uint8_t expected_decrypted_response[16] = {0x01, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
                                             0xA5, salt, salt, salt, salt, salt,
                                             salt, salt, salt, salt};
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_decrypted_response[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  // Seeker sends pairing request
  nearby_test_fakes_SimulatePairing(kRemoteDevice);

  // BT negotatiates passkey 123456 (0x01E240)
  uint8_t raw_passkey_block[16] = {0x02, 0x01, 0xE2, 0x40};
  uint8_t encrypted_passkey_block[16];
  nearby_test_fakes_Aes128Encrypt(raw_passkey_block, encrypted_passkey_block,
                                  kExpectedAesKey);
  // Seeker sends the passkey to provider
  nearby_fp_fakes_ReceivePasskey(encrypted_passkey_block,
                                 sizeof(encrypted_passkey_block));

  // Provider sends the passkey to seeker
  response = nearby_test_fakes_GetGattNotifications().at(kPasskey);
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  uint8_t expected_passkey_block[16] = {0x03, 0x01, 0xE2, 0x40, salt, salt,
                                        salt, salt, salt, salt, salt, salt,
                                        salt, salt, salt, salt};
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_passkey_block[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  ASSERT_EQ(123456, nearby_test_fakes_GetRemotePasskey());
  ASSERT_EQ(kRemoteDevice, nearby_test_fakes_GetPairedDevice());

  // Seeker sends their account key
  WriteToAccountKey();

  std::cout << "Account keys: "
            << VecToString(nearby_test_fakes_GetRawAccountKeys()) << std::endl;
  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(1, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);
}

TEST(NearbyFpClient, Pairing_SeekerInitiatedWriteKeyEarly_PairingSuccessful) {
  uint8_t salt = 0xAB;

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_Init(NULL));
  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kBobPrivateKey, kBobPublicKey));
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_DISCOVERABLE));
  nearby_test_fakes_SetRandomNumber(salt);

  // Provider sets the advertisement
  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kDiscoverableAdvertisement));

  uint8_t request[16];
  request[0] = 0x00;  // key-based pairing request
  request[1] = 0x00;  // bit 1 (msb) cleared
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // salt
  for (int i = 8; i < 16; i++) {
    request[i] = 0xCD + i;
  }
  uint8_t encrypted[16 + 64];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);
  // Seeker responds
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  auto response = nearby_test_fakes_GetGattNotifications().at(kKeyBasedPairing);
  std::cout << VecToString(response) << std::endl;
  uint8_t decrypted_response[16];
  uint8_t expected_decrypted_response[16] = {0x01, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
                                             0xA5, salt, salt, salt, salt, salt,
                                             salt, salt, salt, salt};
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_decrypted_response[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  // Seeker sends pairing request
  nearby_test_fakes_SimulatePairing(kRemoteDevice);

  // Seeker sends their account key
  WriteToAccountKey();

  // BT negotatiates passkey 123456 (0x01E240)
  uint8_t raw_passkey_block[16] = {0x02, 0x01, 0xE2, 0x40};
  uint8_t encrypted_passkey_block[16];
  nearby_test_fakes_Aes128Encrypt(raw_passkey_block, encrypted_passkey_block,
                                  kExpectedAesKey);
  // Seeker sends the passkey to provider
  nearby_fp_fakes_ReceivePasskey(encrypted_passkey_block,
                                 sizeof(encrypted_passkey_block));

  // Provider sends the passkey to seeker
  response = nearby_test_fakes_GetGattNotifications().at(kPasskey);
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  uint8_t expected_passkey_block[16] = {0x03, 0x01, 0xE2, 0x40, salt, salt,
                                        salt, salt, salt, salt, salt, salt,
                                        salt, salt, salt, salt};
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_passkey_block[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  ASSERT_EQ(123456, nearby_test_fakes_GetRemotePasskey());
  ASSERT_EQ(kRemoteDevice, nearby_test_fakes_GetPairedDevice());

  std::cout << "Account keys: "
            << VecToString(nearby_test_fakes_GetRawAccountKeys()) << std::endl;
  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(1, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);
}

TEST(NearbyFpClient, Pair_AccountKeyStorageFull_AddsNewKey) {
  uint8_t account_keys[] = {
      5,    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xAA,
      0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44,
      0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88, 0x03, 0x13, 0x23,
      0x33, 0x43, 0x53, 0x63, 0x73, 0x83, 0x93, 0xA3, 0xB3, 0xC3, 0xD3, 0xE3,
      0xF3, 0x04, 0x14, 0x24, 0x34, 0x44, 0x54, 0x64, 0x74, 0x84, 0x94, 0xA4,
      0xB4, 0xC4, 0xD4, 0xE4, 0xF4, 0x05, 0x15, 0x25, 0x35, 0x45, 0x55, 0x65,
      0x75, 0x85, 0x95, 0xA5, 0xB5, 0xC5, 0xD5, 0xE5, 0xF5,
  };
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetAccountKeys(account_keys, sizeof(account_keys));
  nearby_fp_LoadAccountKeys();

  Pair(0x40);

  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(5, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);
}

TEST(NearbyFpClient, ReadModelId) {
  std::vector<uint8_t> expected_model = {0x10, 0x11, 0x12};
  uint8_t model[3];
  size_t length = sizeof(model);
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_Init(NULL));

  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_GattReadModelId(model, &length));

  ASSERT_EQ(sizeof(model), length);
  ASSERT_EQ(expected_model, std::vector<uint8_t>(model, model + length));
}

TEST(NearbyFpClient, Aes128Encrypt) {
  uint8_t input[16] = {0xF3, 0x0F, 0x4E, 0x78, 0x6C, 0x59, 0xA7, 0xBB,
                       0xF3, 0x87, 0x3B, 0x5A, 0x49, 0xBA, 0x97, 0xEA};
  uint8_t key[16] = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6,
                     0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};
  uint8_t expected_output[16] = {0xAC, 0x9A, 0x16, 0xF0, 0x95, 0x3A,
                                 0x3F, 0x22, 0x3D, 0xD1, 0x0C, 0xF5,
                                 0x36, 0xE0, 0x9E, 0x9C};
  uint8_t output[16];

  ASSERT_EQ(kNearbyStatusOK,
            nearby_test_fakes_Aes128Encrypt(input, output, key));

  for (int i = 0; i < sizeof(expected_output); i++) {
    ASSERT_EQ(expected_output[i], output[i]) << "Difference at position: " << i;
  }
}

#ifdef NEARBY_FP_ENABLE_ADDITIONAL_DATA
TEST(NearbyFpClient, HmacSha256) {
  const uint8_t kData[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xEE,
                           0x4A, 0x24, 0x83, 0x73, 0x80, 0x52, 0xE4, 0x4E, 0x9B,
                           0x2A, 0x14, 0x5E, 0x5D, 0xDF, 0xAA, 0x44, 0xB9, 0xE5,
                           0x53, 0x6A, 0xF4, 0x38, 0xE1, 0xE5, 0xC6};
  const uint8_t kKey[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                          0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
  const uint8_t kExpectedResult[] = {
      0x55, 0xEC, 0x5E, 0x60, 0x55, 0xAF, 0x6E, 0x92, 0x61, 0x8B, 0x7D,
      0x87, 0x10, 0xD4, 0x41, 0x37, 0x09, 0xAB, 0x5D, 0xA2, 0x7C, 0xA2,
      0x6A, 0x66, 0xF5, 0x2E, 0x5A, 0xD4, 0xE8, 0x20, 0x90, 0x52};
  uint8_t result[32];

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_HmacSha256(result, kKey, sizeof(kKey),
                                                  kData, sizeof(kData)));

  ASSERT_THAT(result, ElementsAreArray(kExpectedResult));
}

TEST(NearbyFpClient, AesCtr) {
  uint8_t message[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xEE,
                       0x4A, 0x24, 0x83, 0x73, 0x80, 0x52, 0xE4, 0x4E, 0x9B,
                       0x2A, 0x14, 0x5E, 0x5D, 0xDF, 0xAA, 0x44, 0xB9, 0xE5,
                       0x53, 0x6A, 0xF4, 0x38, 0xE1, 0xE5, 0xC6};
  const uint8_t kKey[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                          0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
  const uint8_t kExpectedResult[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x53, 0x6F, 0x6D, 0x65,
      0x6F, 0x6E, 0x65, 0x27, 0x73, 0x20, 0x47, 0x6F, 0x6F, 0x67, 0x6C, 0x65,
      0x20, 0x48, 0x65, 0x61, 0x64, 0x70, 0x68, 0x6F, 0x6E, 0x65};

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_AesCtr(message, sizeof(message), kKey));

  ASSERT_THAT(message, ElementsAreArray(kExpectedResult));
}

TEST(NearbyFpClient, DecodeAdditionalData) {
  uint8_t message[] = {0x55, 0xEC, 0x5E, 0x60, 0x55, 0xAF, 0x6E, 0x92, 0x00,
                       0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xEE, 0x4A,
                       0x24, 0x83, 0x73, 0x80, 0x52, 0xE4, 0x4E, 0x9B, 0x2A,
                       0x14, 0x5E, 0x5D, 0xDF, 0xAA, 0x44, 0xB9, 0xE5, 0x53,
                       0x6A, 0xF4, 0x38, 0xE1, 0xE5, 0xC6};
  const uint8_t kKey[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                          0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
  const uint8_t kExpectedResult[] = {
      0x55, 0xEC, 0x5E, 0x60, 0x55, 0xAF, 0x6E, 0x92, 0x00, 0x01, 0x02,
      0x03, 0x04, 0x05, 0x06, 0x07, 0x53, 0x6F, 0x6D, 0x65, 0x6F, 0x6E,
      0x65, 0x27, 0x73, 0x20, 0x47, 0x6F, 0x6F, 0x67, 0x6C, 0x65, 0x20,
      0x48, 0x65, 0x61, 0x64, 0x70, 0x68, 0x6F, 0x6E, 0x65};

  ASSERT_EQ(kNearbyStatusOK,
            nearby_fp_DecodeAdditionalData(message, sizeof(message), kKey));

  ASSERT_THAT(message, ElementsAreArray(kExpectedResult));
}

TEST(NearbyFpClient, EncodeAdditionalData) {
  uint8_t message[] = {0,    0,    0,    0,    0,    0,    0,    0,    0,
                       0,    0,    0,    0,    0,    0,    0,    0x53, 0x6F,
                       0x6D, 0x65, 0x6F, 0x6E, 0x65, 0x27, 0x73, 0x20, 0x47,
                       0x6F, 0x6F, 0x67, 0x6C, 0x65, 0x20, 0x48, 0x65, 0x61,
                       0x64, 0x70, 0x68, 0x6F, 0x6E, 0x65};
  std::vector<uint8_t> random_numbers = {0, 1, 2, 3, 4, 5, 6, 7};
  const uint8_t kKey[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                          0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
  const uint8_t kExpectedResult[] = {
      0x55, 0xEC, 0x5E, 0x60, 0x55, 0xAF, 0x6E, 0x92, 0x00, 0x01, 0x02,
      0x03, 0x04, 0x05, 0x06, 0x07, 0xEE, 0x4A, 0x24, 0x83, 0x73, 0x80,
      0x52, 0xE4, 0x4E, 0x9B, 0x2A, 0x14, 0x5E, 0x5D, 0xDF, 0xAA, 0x44,
      0xB9, 0xE5, 0x53, 0x6A, 0xF4, 0x38, 0xE1, 0xE5, 0xC6};
  nearby_test_fakes_SetRandomNumberSequence(random_numbers);

  ASSERT_EQ(kNearbyStatusOK,
            nearby_fp_EncodeAdditionalData(message, sizeof(message), kKey));

  ASSERT_THAT(message, ElementsAreArray(kExpectedResult));
}

TEST(NearbyFpClient, PairAndGetPersonalizedName) {
  uint8_t name[] = {0x53, 0x6F, 0x6D, 0x65, 0x6F, 0x6E, 0x65, 0x27, 0x73,
                    0x20, 0x47, 0x6F, 0x6F, 0x67, 0x6C, 0x65, 0x20, 0x48,
                    0x65, 0x61, 0x64, 0x70, 0x68, 0x6F, 0x6E, 0x65};
  nearby_fp_client_Init(NULL);
  ASSERT_EQ(kNearbyStatusOK,
            nearby_platform_SaveValue(kStoredKeyPersonalizedName, name,
                                      sizeof(name)));

  Pair(0x60);

  auto additional_data =
      nearby_test_fakes_GetGattNotifications().at(kAdditionalData);
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_DecodeAdditionalData(
                                 additional_data.data(), additional_data.size(),
                                 kExpectedAesKey));
  ASSERT_THAT(std::vector<uint8_t>(
                  additional_data.begin() + ADDITIONAL_DATA_HEADER_SIZE,
                  additional_data.end()),
              ElementsAreArray(name));
}

TEST(NearbyFpClient, PairAndSetPersonalizedName) {
  uint8_t name[] = {0,    0,    0,    0,    0,    0,    0,    0,    0,
                    0,    0,    0,    0,    0,    0,    0,    0x53, 0x6F,
                    0x6D, 0x65, 0x6F, 0x6E, 0x65, 0x27, 0x73, 0x20, 0x47,
                    0x6F, 0x6F, 0x67, 0x6C, 0x65, 0x20, 0x48, 0x65, 0x61,
                    0x64, 0x70, 0x68, 0x6F, 0x6E, 0x65};
  nearby_fp_EncodeAdditionalData(name, sizeof(name), kSeekerAccountKey);
  uint8_t request[16];
  request[0] = 0x10;  // action request
  request[1] = 0x40;  // additional data characteristic
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  request[8] = 0;   // message group, ignored
  request[9] = 0;   // message code, ignored
  request[10] = 1;  // data ID, personalized name
  // salt
  request[11] = 0x67;
  request[12] = 0x89;
  request[13] = 0xAB;
  request[14] = 0xCD;
  request[15] = 0xEF;
  uint8_t encrypted[16];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kSeekerAccountKey);
  nearby_fp_client_Init(NULL);
  Pair(0x40);

  // Seeker wants to set personalized name
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));
  nearby_fp_fakes_ReceiveAdditionalData(name, sizeof(name));

  uint8_t result[sizeof(name) - ADDITIONAL_DATA_HEADER_SIZE];
  size_t length = sizeof(result);
  ASSERT_EQ(kNearbyStatusOK, nearby_platform_LoadValue(
                                 kStoredKeyPersonalizedName, result, &length));
  ASSERT_EQ(length, sizeof(result));
  ASSERT_THAT(std::vector<uint8_t>(name + ADDITIONAL_DATA_HEADER_SIZE,
                                   name + sizeof(name)),
              ElementsAreArray(result));
}
#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */

#ifdef NEARBY_FP_MESSAGE_STREAM
TEST(NearbyFpClient, RfcommConnected_NoBatteryInfo_SendsModelIdAndBleAddress) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kExpectedRfcommOutput[] = {// Model Id
                                               3, 1, 0, 3, 0x10, 0x11, 0x12,
                                               // Ble Address
                                               3, 2, 0, 6, 0x6b, 0xab, 0xab,
                                               0xab, 0xab, 0xab};
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);

  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  ASSERT_EQ(1, message_stream_events.size());
  ASSERT_EQ(MessageStreamConnectedEvent(kPeerAddress),
            *message_stream_events[0]);
  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}

#ifdef NEARBY_FP_RETROACTIVE_PAIRING
TEST(NearbyFpClient, RetroactivePair) {
  nearby_fp_client_Init(NULL);
  constexpr uint64_t kPeerAddress = 0xB0B1B2B3B4B5;
  nearby_test_fakes_DevicePaired(kPeerAddress);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusOK);

  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  uint8_t salt = 0xAB;

  nearby_test_fakes_SetAntiSpoofingKey(kBobPrivateKey, kBobPublicKey);
  nearby_test_fakes_SetRandomNumber(salt);

  uint8_t request[16];
  request[0] = 0x00;  // key-based pairing request
  request[1] = 1 << 4;
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // Seeker's address
  request[8] = 0xB0;
  request[9] = 0xB1;
  request[10] = 0xB2;
  request[11] = 0xB3;
  request[12] = 0xB4;
  request[13] = 0xB5;
  // salt
  request[14] = 0xCD;
  request[15] = 0xEF;
  uint8_t encrypted[16 + 64];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);

  // Seeker responds
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  // Seeker sends their account key
  uint8_t encrypted_account_key_write_request[16];
  nearby_test_fakes_Aes128Encrypt(
      kSeekerAccountKey, encrypted_account_key_write_request, kExpectedAesKey);
  nearby_fp_fakes_ReceiveAccountKeyWrite(
      encrypted_account_key_write_request,
      sizeof(encrypted_account_key_write_request));

  std::cout << "Account keys: "
            << VecToString(nearby_test_fakes_GetRawAccountKeys()) << std::endl;
  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(1, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);
}

TEST(NearbyFpClient, RetroactivePairAfterInitialPair) {
  constexpr uint64_t kPeerAddress = 0xB0B1B2B3B4B5;
  uint8_t salt = 0xAB;

  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_Init(NULL));
  ASSERT_EQ(kNearbyStatusOK, nearby_test_fakes_SetAntiSpoofingKey(
                                 kBobPrivateKey, kBobPublicKey));
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_client_SetAdvertisement(
                                 NEARBY_FP_ADVERTISEMENT_DISCOVERABLE));
  nearby_test_fakes_SetRandomNumber(salt);

  // Provider sets the advertisement
  ASSERT_THAT(nearby_test_fakes_GetAdvertisement(),
              ElementsAreArray(kDiscoverableAdvertisement));

  uint8_t request[16];
  request[0] = 0x00;  // key-based pairing request
  request[1] = 0x40;  // bit 1 (msb) set
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // Seeker's address
  request[8] = 0xB0;
  request[9] = 0xB1;
  request[10] = 0xB2;
  request[11] = 0xB3;
  request[12] = 0xB4;
  request[13] = 0xB5;
  // salt
  request[14] = 0xCD;
  request[15] = 0xEF;
  uint8_t encrypted[16 + 64];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);
  // Seeker responds
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  auto response = nearby_test_fakes_GetGattNotifications().at(kKeyBasedPairing);
  std::cout << VecToString(response) << std::endl;
  uint8_t decrypted_response[16];
  uint8_t expected_decrypted_response[16] = {0x01, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
                                             0xA5, salt, salt, salt, salt, salt,
                                             salt, salt, salt, salt};
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_decrypted_response[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  // Provider sends pairing request
  ASSERT_EQ(kRemoteDevice, nearby_test_fakes_GetPairingRequestAddress());

  // BT negotatiates passkey 123456 (0x01E240)
  uint8_t raw_passkey_block[16] = {0x02, 0x01, 0xE2, 0x40};
  uint8_t encrypted_passkey_block[16];
  nearby_test_fakes_Aes128Encrypt(raw_passkey_block, encrypted_passkey_block,
                                  kExpectedAesKey);
  // Seeker sends the passkey to provider
  nearby_fp_fakes_ReceivePasskey(encrypted_passkey_block,
                                 sizeof(encrypted_passkey_block));

  // Provider sends the passkey to seeker
  response = nearby_test_fakes_GetGattNotifications().at(kPasskey);
  nearby_test_fakes_Aes128Decrypt(response.data(), decrypted_response,
                                  kExpectedAesKey);
  uint8_t expected_passkey_block[16] = {0x03, 0x01, 0xE2, 0x40, salt, salt,
                                        salt, salt, salt, salt, salt, salt,
                                        salt, salt, salt, salt};
  for (int i = 0; i < sizeof(expected_decrypted_response); i++) {
    ASSERT_EQ(expected_passkey_block[i], decrypted_response[i])
        << "Difference at position: " << i;
  }

  ASSERT_EQ(123456, nearby_test_fakes_GetRemotePasskey());
  ASSERT_EQ(kRemoteDevice, nearby_test_fakes_GetPairedDevice());

  // Seeker sends their account key
  WriteToAccountKey();

  std::cout << "Account keys: "
            << VecToString(nearby_test_fakes_GetRawAccountKeys()) << std::endl;
  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(1, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);

  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusOK);

  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  // Retroactive pairing

  request[0] = 0x00;  // key-based pairing request
  request[1] = 1 << 4;
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // Seeker's address
  request[8] = 0xB0;
  request[9] = 0xB1;
  request[10] = 0xB2;
  request[11] = 0xB3;
  request[12] = 0xB4;
  request[13] = 0xB5;
  // salt
  request[14] = 0xCD;
  request[15] = 0xEF;
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);

  // Seeker responds
  ASSERT_NE(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  // Seeker sends their account key
  uint8_t encrypted_account_key_write_request[16];
  nearby_test_fakes_Aes128Encrypt(
      kSeekerAccountKey2, encrypted_account_key_write_request, kExpectedAesKey);
  ASSERT_NE(kNearbyStatusOK, nearby_fp_fakes_ReceiveAccountKeyWrite(
                                 encrypted_account_key_write_request,
                                 sizeof(encrypted_account_key_write_request)));

  keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(1, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);
}

TEST(NearbyFpClient, RetroactivePairTwice) {
  nearby_fp_client_Init(NULL);
  constexpr uint64_t kPeerAddress = 0xB0B1B2B3B4B5;
  nearby_test_fakes_DevicePaired(kPeerAddress);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusOK);

  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  uint8_t salt = 0xAB;

  nearby_test_fakes_SetAntiSpoofingKey(kBobPrivateKey, kBobPublicKey);
  nearby_test_fakes_SetRandomNumber(salt);

  uint8_t request[16];
  request[0] = 0x00;  // key-based pairing request
  request[1] = 1 << 4;
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // Seeker's address
  request[8] = 0xB0;
  request[9] = 0xB1;
  request[10] = 0xB2;
  request[11] = 0xB3;
  request[12] = 0xB4;
  request[13] = 0xB5;
  // salt
  request[14] = 0xCD;
  request[15] = 0xEF;
  uint8_t encrypted[16 + 64];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);

  // Seeker responds
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  // Seeker sends their account key
  uint8_t encrypted_account_key_write_request[16];
  nearby_test_fakes_Aes128Encrypt(
      kSeekerAccountKey, encrypted_account_key_write_request, kExpectedAesKey);
  ASSERT_EQ(kNearbyStatusOK, nearby_fp_fakes_ReceiveAccountKeyWrite(
                                 encrypted_account_key_write_request,
                                 sizeof(encrypted_account_key_write_request)));

  std::cout << "Account keys: "
            << VecToString(nearby_test_fakes_GetRawAccountKeys()) << std::endl;
  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(1, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);

  nearby_test_fakes_SetRandomNumber(salt);
  request[0] = 0x00;  // key-based pairing request
  request[1] = 1 << 4;
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // Seeker's address
  request[8] = 0xB0;
  request[9] = 0xB1;
  request[10] = 0xB2;
  request[11] = 0xB3;
  request[12] = 0xB4;
  request[13] = 0xB5;
  // salt
  request[14] = 0xCD;
  request[15] = 0xEF;
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);

  // Seeker responds
  ASSERT_NE(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  // Seeker sends their account key
  nearby_test_fakes_Aes128Encrypt(
      kSeekerAccountKey2, encrypted_account_key_write_request, kExpectedAesKey);
  ASSERT_NE(kNearbyStatusOK, nearby_fp_fakes_ReceiveAccountKeyWrite(
                                 encrypted_account_key_write_request,
                                 sizeof(encrypted_account_key_write_request)));

  keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(1, keys.size());
  ASSERT_EQ(std::vector<uint8_t>(kSeekerAccountKey,
                                 kSeekerAccountKey + sizeof(kExpectedAesKey)),
            keys.GetKeys()[0]);
}

TEST(NearbyFpClient, RetroactivePairWrongBtAddress) {
  nearby_fp_client_Init(NULL);
  constexpr uint64_t kPeerAddress = 0xB0B1B2B3B4B5;
  nearby_test_fakes_DevicePaired(kPeerAddress);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusOK);

  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  uint8_t salt = 0xAB;

  nearby_test_fakes_SetAntiSpoofingKey(kBobPrivateKey, kBobPublicKey);
  nearby_test_fakes_SetRandomNumber(salt);

  uint8_t request[16];
  request[0] = 0x00;  // key-based pairing request
  request[1] = 1 << 4;
  // Provider's public address
  request[2] = 0xA0;
  request[3] = 0xA1;
  request[4] = 0xA2;
  request[5] = 0xA3;
  request[6] = 0xA4;
  request[7] = 0xA5;
  // Seeker's address
  request[8] = 0xC0;
  request[9] = 0xC1;
  request[10] = 0xC2;
  request[11] = 0xC3;
  request[12] = 0xC4;
  request[13] = 0xC5;
  // salt
  request[14] = 0xCD;
  request[15] = 0xEF;
  uint8_t encrypted[16 + 64];
  nearby_test_fakes_Aes128Encrypt(request, encrypted, kExpectedAesKey);
  memcpy(encrypted + 16, kAlicePublicKey, 64);

  // Seeker responds
  ASSERT_NE(kNearbyStatusOK, nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
                                 encrypted, sizeof(encrypted)));

  // Seeker sends their account key
  uint8_t encrypted_account_key_write_request[16];
  nearby_test_fakes_Aes128Encrypt(
      kSeekerAccountKey, encrypted_account_key_write_request, kExpectedAesKey);
  ASSERT_NE(kNearbyStatusOK, nearby_fp_fakes_ReceiveAccountKeyWrite(
                                 encrypted_account_key_write_request,
                                 sizeof(encrypted_account_key_write_request)));

  auto keys = nearby_test_fakes_GetAccountKeys();
  ASSERT_EQ(0, keys.size());
}
#endif /* NEARBY_FP_RETROACTIVE_PAIRING */

TEST(NearbyFpClient, RfcommConnected_ClientDisconnects_EmitsDisconnectEvent) {
  constexpr uint64_t kPeerAddress = 0x123456;
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_test_fakes_MessageStreamDisconnected(kPeerAddress);

  ASSERT_EQ(2, message_stream_events.size());
  ASSERT_EQ(MessageStreamConnectedEvent(kPeerAddress),
            *message_stream_events[0]);
  ASSERT_EQ(MessageStreamDisconnectedEvent(kPeerAddress),
            *message_stream_events[1]);
}

TEST(NearbyFpClient, RfcommConnected_PeerSendsMessage_PassMessageToClientApp) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kPeerMessage[] = {101, 102, 0, 4, 81, 82, 83, 84};
  const nearby_event_MessageStreamReceived kExpectedMessage = {
      .peer_address = kPeerAddress,
      .message_group = kPeerMessage[0],
      .message_code = kPeerMessage[1],
      .length = kPeerMessage[2] * 256 + kPeerMessage[3],
      .data = (uint8_t*)kPeerMessage + 4,
  };
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);

  nearby_test_fakes_MessageStreamReceived(kPeerAddress, kPeerMessage,
                                          sizeof(kPeerMessage));

  ASSERT_EQ(2, message_stream_events.size());
  ASSERT_EQ(MessageStreamConnectedEvent(kPeerAddress),
            *message_stream_events[0]);
  ASSERT_EQ(MessageStreamReceivedEvent(&kExpectedMessage),
            *message_stream_events[1]);
}

TEST(NearbyFpClient,
     RfcommConnected_ConnectAndDisconnect_CanHandleManySessions) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kPeerMessage[] = {101, 102, 0, 4, 81, 82, 83, 84};
  const nearby_event_MessageStreamReceived kExpectedMessage = {
      .peer_address = kPeerAddress,
      .message_group = kPeerMessage[0],
      .message_code = kPeerMessage[1],
      .length = kPeerMessage[2] * 256 + kPeerMessage[3],
      .data = (uint8_t*)kPeerMessage + 4,
  };
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);

  for (int i = 1; i < NEARBY_MAX_RFCOMM_CONNECTIONS + 10; i++) {
    nearby_test_fakes_MessageStreamConnected(kPeerAddress + i);
    nearby_test_fakes_MessageStreamDisconnected(kPeerAddress + i);
  }
  nearby_test_fakes_MessageStreamConnected(kPeerAddress);
  nearby_test_fakes_MessageStreamReceived(kPeerAddress, kPeerMessage,
                                          sizeof(kPeerMessage));

  int events = message_stream_events.size();
  ASSERT_EQ(MessageStreamConnectedEvent(kPeerAddress),
            *message_stream_events[events - 2]);
  ASSERT_EQ(MessageStreamReceivedEvent(&kExpectedMessage),
            *message_stream_events[events - 1]);
}

#if NEARBY_MAX_RFCOMM_CONNECTIONS > 1
TEST(NearbyFpClient, RfcommConnected_TwoInterleavedConnections_ParsesMessages) {
  constexpr uint64_t kPeerAddress1 = 0x123456;
  constexpr uint8_t kPeerMessage1[] = {101, 102, 0, 4, 81, 82, 83, 84};
  const nearby_event_MessageStreamReceived kExpectedMessage1 = {
      .peer_address = kPeerAddress1,
      .message_group = kPeerMessage1[0],
      .message_code = kPeerMessage1[1],
      .length = kPeerMessage1[2] * 256 + kPeerMessage1[3],
      .data = (uint8_t*)kPeerMessage1 + 4,
  };
  constexpr uint64_t kPeerAddress2 = 0x7890ab;
  constexpr uint8_t kPeerMessage2[] = {201, 202, 0, 5, 91, 92, 93, 94, 95};
  const nearby_event_MessageStreamReceived kExpectedMessage2 = {
      .peer_address = kPeerAddress2,
      .message_group = kPeerMessage2[0],
      .message_code = kPeerMessage2[1],
      .length = kPeerMessage2[2] * 256 + kPeerMessage2[3],
      .data = (uint8_t*)kPeerMessage2 + 4,
  };
  nearby_fp_client_Init(&kClientCallbacks);
  Pair(0x40);
  message_stream_events.clear();
  nearby_test_fakes_GetRfcommOutput().clear();
  nearby_test_fakes_SetGetBatteryInfoResult(kNearbyStatusUnimplemented);
  nearby_test_fakes_MessageStreamConnected(kPeerAddress1);
  nearby_test_fakes_MessageStreamConnected(kPeerAddress2);

  // Send the messages byte by byte, interleaving bytes from both connections
  // to verify that the parses handles the streams separately
  for (int i = 0; i < std::max(sizeof(kPeerMessage1), sizeof(kPeerMessage2));
       i++) {
    if (i < sizeof(kPeerMessage1)) {
      nearby_test_fakes_MessageStreamReceived(kPeerAddress1, kPeerMessage1 + i,
                                              1);
    }
    if (i < sizeof(kPeerMessage2)) {
      nearby_test_fakes_MessageStreamReceived(kPeerAddress2, kPeerMessage2 + i,
                                              1);
    }
  }

  ASSERT_EQ(4, message_stream_events.size());
  ASSERT_EQ(MessageStreamConnectedEvent(kPeerAddress1),
            *message_stream_events[0]);
  ASSERT_EQ(MessageStreamConnectedEvent(kPeerAddress2),
            *message_stream_events[1]);
  ASSERT_EQ(MessageStreamReceivedEvent(&kExpectedMessage1),
            *message_stream_events[2]);
  ASSERT_EQ(MessageStreamReceivedEvent(&kExpectedMessage2),
            *message_stream_events[3]);
}
#endif /* NEARBY_MAX_RFCOMM_CONNECTIONS > 1 */

TEST(NearbyFpClient, SendMessageStreamMessage) {
  constexpr nearby_message_stream_Message kMessage{
      .message_group = 20,
      .message_code = 10,
  };
  constexpr uint8_t kExpectedRfcommOutput[] = {20, 10, 0, 0};
  nearby_fp_client_Init(&kClientCallbacks);
  nearby_test_fakes_GetRfcommOutput().clear();

  nearby_fp_client_SendMessage(0x123456, &kMessage);

  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}

TEST(NearbyFpClient, SendAck) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr nearby_event_MessageStreamReceived kMessage{
      .peer_address = kPeerAddress,
      .message_group = 20,
      .message_code = 10,
  };
  constexpr uint8_t kExpectedRfcommOutput[] = {0xFF, 1, 0, 2, 20, 10};
  nearby_fp_client_Init(&kClientCallbacks);
  nearby_test_fakes_GetRfcommOutput().clear();

  nearby_fp_client_SendAck(&kMessage);

  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}

TEST(NearbyFpClient, SendNack) {
  constexpr uint64_t kPeerAddress = 0x123456;
  constexpr uint8_t kFailReason = 30;
  constexpr nearby_event_MessageStreamReceived kMessage{
      .peer_address = kPeerAddress,
      .message_group = 20,
      .message_code = 10,
  };
  constexpr uint8_t kExpectedRfcommOutput[] = {0xFF,        2,  0, 3,
                                               kFailReason, 20, 10};
  nearby_fp_client_Init(&kClientCallbacks);
  nearby_test_fakes_GetRfcommOutput().clear();

  nearby_fp_client_SendNack(&kMessage, kFailReason);

  ASSERT_THAT(kExpectedRfcommOutput,
              ElementsAreArray(nearby_test_fakes_GetRfcommOutput()));
}

#endif /* NEARBY_FP_MESSAGE_STREAM */

TEST(NearbyFpClient, TimerTriggered_RotatesBleAddress) {
  uint64_t firstAddress, secondAddress, thirdAddress;
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetInPairingMode(false);
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_DISCOVERABLE);
  firstAddress = nearby_platform_GetBleAddress();

  nearby_test_fakes_SetRandomNumber(30);
  nearby_test_fakes_SetCurrentTimeMs(nearby_test_fakes_GetNextTimerMs());
  secondAddress = nearby_platform_GetBleAddress();
  nearby_test_fakes_SetRandomNumber(31);
  nearby_test_fakes_SetCurrentTimeMs(nearby_test_fakes_GetNextTimerMs());
  thirdAddress = nearby_platform_GetBleAddress();

  ASSERT_NE(firstAddress, secondAddress);
  ASSERT_NE(secondAddress, thirdAddress);
}

TEST(NearbyFpClient, TimerTriggered_InPairingMode_DoesntRotateBleAddress) {
  uint64_t firstAddress, secondAddress, thirdAddress;
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetInPairingMode(false);
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE);
  firstAddress = nearby_platform_GetBleAddress();

  nearby_test_fakes_SetInPairingMode(true);
  nearby_test_fakes_SetRandomNumber(30);
  nearby_test_fakes_SetCurrentTimeMs(nearby_test_fakes_GetNextTimerMs());
  secondAddress = nearby_platform_GetBleAddress();

  nearby_test_fakes_SetInPairingMode(false);
  nearby_test_fakes_SetRandomNumber(31);
  nearby_test_fakes_SetCurrentTimeMs(nearby_test_fakes_GetNextTimerMs());
  thirdAddress = nearby_platform_GetBleAddress();

  ASSERT_EQ(firstAddress, secondAddress);
  ASSERT_NE(secondAddress, thirdAddress);
}

TEST(NearbyFpClient, AdvertiseDisoverable_RotatesBleAddress) {
  uint64_t firstAddress;
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetInPairingMode(false);
  firstAddress = nearby_platform_GetBleAddress();

  nearby_test_fakes_SetRandomNumber(32);
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_DISCOVERABLE);

  ASSERT_NE(firstAddress, nearby_platform_GetBleAddress());
}

TEST(NearbyFpClient,
     AdvertiseDisoverable_InPairingMode_DoesntRotatesBleAddress) {
  uint64_t firstAddress;
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetInPairingMode(true);
  firstAddress = nearby_platform_GetBleAddress();

  nearby_test_fakes_SetRandomNumber(32);
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_DISCOVERABLE);

  ASSERT_EQ(firstAddress, nearby_platform_GetBleAddress());
}

TEST(NearbyFpClient,
     ChangeAdvertisementType_InPairingMode_DoesntRotateBleAddress) {
  uint64_t firstAddress;
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetInPairingMode(true);
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE);
  firstAddress = nearby_platform_GetBleAddress();

  nearby_test_fakes_SetRandomNumber(34);
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_DISCOVERABLE);

  ASSERT_EQ(firstAddress, nearby_platform_GetBleAddress());
}

TEST(NearbyFpClient, ChangeAdvertisementFlags_DoesntRotateBleAddress) {
  uint64_t firstAddress;
  nearby_fp_client_Init(NULL);
  nearby_test_fakes_SetInPairingMode(false);
  nearby_fp_client_SetAdvertisement(NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE);
  firstAddress = nearby_platform_GetBleAddress();

  nearby_test_fakes_SetRandomNumber(36);
  nearby_fp_client_SetAdvertisement(
      NEARBY_FP_ADVERTISEMENT_NON_DISCOVERABLE |
      NEARBY_FP_ADVERTISEMENT_PAIRING_UI_INDICATOR);

  ASSERT_EQ(firstAddress, nearby_platform_GetBleAddress());
}

#pragma GCC diagnostic pop

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
