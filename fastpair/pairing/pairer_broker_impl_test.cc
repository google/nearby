// Copyright 2023 Google LLC
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

#include "fastpair/pairing/pairer_broker_impl.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "fastpair//handshake/fast_pair_handshake_lookup.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_data_encryptor_impl.h"
#include "fastpair/handshake/fast_pair_handshake_impl.h"
#include "fastpair/handshake/fast_pair_handshake_lookup.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/pairing/fastpair/fast_pair_pairer.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "internal/base/bluetooth_address.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"
#include <openssl/rand.h>

namespace nearby {
namespace fastpair {
namespace {
using Property = nearby::api::ble_v2::GattCharacteristic::Property;
using Permission = nearby::api::ble_v2::GattCharacteristic::Permission;
using ::nearby::api::ble_v2::GattCharacteristic;
using DiscoveryCallback = BluetoothClassicMedium::DiscoveryCallback;

constexpr absl::string_view kMetadataId("718c17");
constexpr absl::string_view kPublicAntiSpoof =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
constexpr std::array<uint8_t, kAesBlockByteSize> kRawResponseBytes = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D,
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6};
constexpr Uuid kFastPairServiceUuid(0x0000FE2C00001000, 0x800000805F9B34FB);
constexpr Uuid kKeyBasedCharacteristicUuidV2(0xFE2C123483664814,
                                             0x8EB001DE32100BEA);
constexpr Uuid kPasskeyCharacteristicUuidV2(0xFE2C123583664814,
                                            0x8EB001DE32100BEA);
constexpr Uuid kAccountKeyCharacteristicUuidV2(0xFE2C123683664814,
                                               0x8EB001DE32100BEA);
constexpr absl::string_view kPasskey("123456");
constexpr absl::string_view kWrongResponse("wrongresponse");
constexpr absl::Duration kWaitTimeout = absl::Milliseconds(200);
struct CharacteristicData {
  // Write result returned to the gatt client.
  absl::Status write_result = absl::OkStatus();
};
}  // namespace

class PairerBrokerObserver : public PairerBroker::Observer {
 public:
  PairerBrokerObserver(PairerBroker* pairer_broker,
                       CountDownLatch* device_paired_latch,
                       CountDownLatch* account_key_writed_latch,
                       CountDownLatch* pairing_completed_latch,
                       CountDownLatch* pairing_failure_latch)
      : pairer_broker_(pairer_broker),
        device_paired_latch_(device_paired_latch),
        account_key_writed_latch_(account_key_writed_latch),
        pairing_completed_latch_(pairing_completed_latch),
        pairing_failure_latch_(pairing_failure_latch) {
    pairer_broker_->AddObserver(this);
  }

  ~PairerBrokerObserver() override { pairer_broker_->RemoveObserver(this); }

  void OnHandshakeComplete(FastPairDevice& device) override {
    NEARBY_LOGS(INFO) << __func__;
    handshake_completed_ = true;
  }

  void OnDevicePaired(FastPairDevice& device) override {
    NEARBY_LOGS(INFO) << __func__;
    device_paired_latch_->CountDown();
  }

  void OnAccountKeyWrite(FastPairDevice& device,
                         std::optional<PairFailure> error) override {
    NEARBY_LOGS(INFO) << __func__;
    if (error.has_value()) {
      account_key_failure_ = error.value();
    }
    account_key_writed_latch_->CountDown();
  }

  void OnPairingComplete(FastPairDevice& device) override {
    NEARBY_LOGS(INFO) << __func__;
    pairing_completed_latch_->CountDown();
  }

  void OnPairFailure(FastPairDevice& device, PairFailure failure) override {
    pair_failure_ = failure;
    pairing_failure_latch_->CountDown();
  }

  PairerBroker* pairer_broker_;
  bool handshake_completed_ = false;
  CountDownLatch* device_paired_latch_ = nullptr;
  CountDownLatch* account_key_writed_latch_ = nullptr;
  CountDownLatch* pairing_completed_latch_ = nullptr;
  CountDownLatch* pairing_failure_latch_ = nullptr;
  PairFailure account_key_failure_ = PairFailure::kUnknown;
  PairFailure pair_failure_ = PairFailure::kUnknown;
};

class PairerBrokerImplTest : public testing::Test {
 public:
  void SetUp() override {
    env_.Start();
    // Setups seeker device.
    mediums_ = std::make_unique<Mediums>();

    // Setups provider device.
    adapter_provider_ = std::make_unique<BluetoothAdapter>();
    adapter_provider_->SetStatus(BluetoothAdapter::Status::kEnabled);
    adapter_provider_->SetName("Device-Provider");
    adapter_provider_->SetScanMode(
        BluetoothAdapter::ScanMode::kConnectableDiscoverable);
    bt_provider_ = std::make_unique<BluetoothClassicMedium>(*adapter_provider_);

    // Discovering provider device.
    CountDownLatch found_latch(1);
    mediums_->GetBluetoothClassic().GetMedium().StartDiscovery(
        DiscoveryCallback{.device_discovered_cb = [&](BluetoothDevice& device) {
          remote_device_ = &device;
          found_latch.CountDown();
        }});
    found_latch.Await();
    env_.Sync();
  }

  void TearDown() override {
    env_.Sync(false);
    executor_.Shutdown();
    pairer_broker_.reset();
    FastPairHandshakeLookup::GetInstance()->Clear();
    mediums_.reset();
    device_.reset();
    repository_.reset();
    handshake_ = nullptr;
    remote_device_ = nullptr;
    key_based_characteristic_ = std::nullopt;
    passkey_characteristic_ = std::nullopt;

    adapter_provider_->SetStatus(BluetoothAdapter::Status::kDisabled);
    gatt_server_->Stop();
    gatt_server_.reset();
    bt_provider_.reset();
    ble_provider_.reset();
    env_.Sync(false);
    adapter_provider_.reset();
    env_.Stop();
  }

  void CreateMockDevice(DeviceFastPairVersion version, Protocol protocol) {
    device_ = std::make_unique<FastPairDevice>(
        kMetadataId, remote_device_->GetMacAddress(), protocol);
    device_->SetVersion(version);
    if (version == DeviceFastPairVersion::kV1) {
      device_->SetPublicAddress(remote_device_->GetMacAddress());
    }
    if (protocol == Protocol::kFastPairSubsequentPairing) {
      device_->SetAccountKey(AccountKey(account_key_));
    }
  }

  void ConfigurePairingContext() {
    api::PairingParams pairing_params;
    pairing_params.pairing_type =
        api::PairingParams::PairingType::kConfirmPasskey;
    pairing_params.passkey = kPasskey;
    env_.ConfigBluetoothPairingContext(&remote_device_->GetImpl(),
                                       pairing_params);
  }

  void CreateFastPairHandshakeInstanceForDevice() {
    FastPairHandshakeLookup::SetCreateFunctionForTesting(absl::bind_front(
        &PairerBrokerImplTest::CreateConnectedHandshake, this));

    CountDownLatch latch(1);
    EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Create(
        *device_, *mediums_,
        [&](FastPairDevice& cb_device, std::optional<PairFailure> failure) {
          EXPECT_EQ(device_.get(), &cb_device);
          EXPECT_EQ(failure, std::nullopt);
          latch.CountDown();
        },
        &executor_));
    latch.Await();
    EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_.get()));
    EXPECT_TRUE(handshake_->completed_successfully());
    EXPECT_EQ(
        device_->GetPublicAddress().value(),
        device::CanonicalizeBluetoothAddress(remote_device_->GetMacAddress()));
  }

  std::unique_ptr<FastPairHandshake> CreateConnectedHandshake(
      FastPairDevice& device, Mediums& mediums,
      FastPairHandshake::OnCompleteCallback callback) {
    CountDownLatch latch(1);
    auto handshake = std::make_unique<FastPairHandshakeImpl>(
        device, mediums,
        [&](FastPairDevice& callback_device,
            std::optional<PairFailure> failure) {
          callback(callback_device, failure);
          latch.CountDown();
        },
        &executor_);
    handshake_ = handshake.get();
    latch.Await();
    return handshake;
  }

  // Sets up provider's metadata information.
  void SetUpFastPairRepository() {
    repository_ = FakeFastPairRepository::Create(kMetadataId, kPublicAntiSpoof);
  }

  // Sets upprovider's gatt_server.
  void SetupProviderGattServer(
      absl::AnyInvocable<void()> trigger_keybase_value_change,
      absl::AnyInvocable<void()> trigger_passkey_value_change) {
    ble_provider_ = std::make_unique<BleV2Medium>(*adapter_provider_);
    gatt_server_ = ble_provider_->StartGattServer(
        /*ServerGattConnectionCallback=*/{
            .on_characteristic_write_cb =
                [&](const api::ble_v2::BlePeripheral& remote_device,
                    const api::ble_v2::GattCharacteristic& characteristic,
                    int offset, absl::string_view data,
                    BleV2Medium::ServerGattConnectionCallback::
                        WriteValueCallback callback) {
                  auto it = characteristics_.find(characteristic);
                  if (it == characteristics_.end()) {
                    callback(absl::NotFoundError("characteristic not found"));
                    return;
                  }
                  if (characteristic == *key_based_characteristic_) {
                    trigger_keybase_value_change();
                  } else if (characteristic == *passkey_characteristic_) {
                    trigger_passkey_value_change();
                  }
                  callback(it->second.write_result);
                }});
    // Insert fast pair related gatt characteristics
    key_based_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kKeyBasedCharacteristicUuidV2, permissions_,
        properties_);
    characteristics_[*key_based_characteristic_].write_result =
        absl::OkStatus();

    passkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kPasskeyCharacteristicUuidV2, permissions_,
        properties_);
    characteristics_[*passkey_characteristic_].write_result = absl::OkStatus();

    accountkey_characteristic_ = gatt_server_->CreateCharacteristic(
        kFastPairServiceUuid, kAccountKeyCharacteristicUuidV2, permissions_,
        properties_);
    characteristics_[*accountkey_characteristic_].write_result =
        absl::OkStatus();
  }

  // Triggers provider's gatt_server to response with public address
  absl::Status TriggerKeyBasedGattChanged() {
    std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor_unique_ptr;
    FastPairDataEncryptor* fast_pair_data_encryptor_;
    if (device_->GetAccountKey().Ok()) {
      CountDownLatch latch(1);
      FastPairDataEncryptorImpl::Factory::CreateAsync(
          *device_,
          [&](std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor) {
            fast_pair_data_encryptor_unique_ptr =
                std::move(fast_pair_data_encryptor);

            latch.CountDown();
          });
      latch.Await();
      fast_pair_data_encryptor_ = fast_pair_data_encryptor_unique_ptr.get();
    } else {
      fast_pair_data_encryptor_ = handshake_->fast_pair_data_encryptor();
    }
    std::array<uint8_t, kAesBlockByteSize> raw_response = kRawResponseBytes;
    std::array<uint8_t, 6> provider_address_bytes;
    device::ParseBluetoothAddress(
        device_->GetBleAddress(),
        absl::MakeSpan(provider_address_bytes.data(),
                       provider_address_bytes.size()));
    std::copy(provider_address_bytes.begin(), provider_address_bytes.end(),
              std::begin(raw_response) + 1);
    std::array<uint8_t, kAesBlockByteSize> encryptedResponse =
        fast_pair_data_encryptor_->EncryptBytes(raw_response);
    std::array<char, kAesBlockByteSize> response;
    std::copy(encryptedResponse.begin(), encryptedResponse.end(),
              response.begin());
    return gatt_server_->NotifyCharacteristicChanged(
        key_based_characteristic_.value(), false, ByteArray(response));
  }

  // Triggers provider's gatt_server to response with passkey
  // success == true, response with correct passkey,
  // otherwise, response with wrong passkey.
  absl::Status TriggerPasskeyGattChanged(absl::string_view pin_code,
                                         uint8_t fast_pair_message_type) {
    std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor_unique_ptr;
    FastPairDataEncryptor* fast_pair_data_encryptor_;
    if (device_->GetAccountKey().Ok()) {
      CountDownLatch latch(1);
      FastPairDataEncryptorImpl::Factory::CreateAsync(
          *device_,
          [&](std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor) {
            fast_pair_data_encryptor_unique_ptr =
                std::move(fast_pair_data_encryptor);

            latch.CountDown();
          });
      latch.Await();
      fast_pair_data_encryptor_ = fast_pair_data_encryptor_unique_ptr.get();
    } else {
      fast_pair_data_encryptor_ = handshake_->fast_pair_data_encryptor();
    }
    std::array<uint8_t, kAesBlockByteSize> raw_response;
    RAND_bytes(raw_response.data(), kAesBlockByteSize);
    raw_response[0] = fast_pair_message_type;
    uint32_t passkey = 0;
    passkey = std::stoi(std::string(pin_code));

    // Need to convert the uint_32 to uint_8 to use in our data vector.
    raw_response[1] = (passkey & 0x00ff0000) >> 16;
    raw_response[2] = (passkey & 0x0000ff00) >> 8;
    raw_response[3] = passkey & 0x000000ff;

    std::array<uint8_t, kAesBlockByteSize> encryptedResponse =
        fast_pair_data_encryptor_->EncryptBytes(raw_response);
    std::array<char, kAesBlockByteSize> response;
    std::copy(encryptedResponse.begin(), encryptedResponse.end(),
              response.begin());
    return gatt_server_->NotifyCharacteristicChanged(
        passkey_characteristic_.value(), false, ByteArray(response));
  }

  absl::Status TriggerPasskeyGattChangedWithWrongResponse() {
    return gatt_server_->NotifyCharacteristicChanged(
        passkey_characteristic_.value(), false,
        ByteArray(std::string(kWrongResponse)));
  }

  bool SetPairingResult(
      std::optional<api::BluetoothPairingCallback::PairingError> error) {
    return env_.SetPairingResult(&remote_device_->GetImpl(), error);
  }

  void SetPasskeyCharacteristicsWriteResultToFailure() {
    auto it = characteristics_.find(*passkey_characteristic_);
    it->second.write_result = absl::UnknownError("Failed to write account key");
  }

  void SetAccountkeyCharacteristicsWriteResultToFailure() {
    auto it = characteristics_.find(*accountkey_characteristic_);
    it->second.write_result = absl::UnknownError("Failed to write account key");
  }

 protected:
  const std::vector<uint8_t> account_key_{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                          0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                          0xCC, 0xDD, 0xEE, 0xFF};
  std::unique_ptr<Mediums> mediums_;
  std::unique_ptr<FastPairDevice> device_;
  BluetoothDevice* remote_device_ = nullptr;
  SingleThreadExecutor executor_;
  std::unique_ptr<PairerBrokerImpl> pairer_broker_;

 private:
  MediumEnvironment& env_{MediumEnvironment::Instance()};
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<BluetoothClassicMedium> bt_provider_;
  std::unique_ptr<BluetoothAdapter> adapter_provider_;
  std::unique_ptr<GattServer> gatt_server_;
  std::unique_ptr<BleV2Medium> ble_provider_;
  FastPairHandshake* handshake_ = nullptr;
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;
  std::optional<GattCharacteristic> accountkey_characteristic_;
  absl::flat_hash_map<GattCharacteristic, CharacteristicData> characteristics_;
  Property properties_ = Property::kWrite | Property::kNotify;
  Permission permissions_ = Permission::kWrite;
};

TEST_F(PairerBrokerImplTest, SuccessInitialPairingWithDeviceV1) {
  ConfigurePairingContext();
  CreateMockDevice(DeviceFastPairVersion::kV1,
                   Protocol::kFastPairInitialPairing);
  bool triggered_keybase_value_change = false;
  bool triggered_passkey_value_change = false;
  SetUpFastPairRepository();
  SetupProviderGattServer(
      [&]() {
        triggered_keybase_value_change = true;
        EXPECT_OK(TriggerKeyBasedGattChanged());
      },
      [&]() {
        triggered_passkey_value_change = true;
        EXPECT_OK(TriggerPasskeyGattChanged(kPasskey, kProviderPasskeyType));
      });
  SetPairingResult(std::nullopt);
  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);
  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_);
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  pairing_completed_latch.Await();
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(triggered_keybase_value_change);
  EXPECT_FALSE(triggered_passkey_value_change);
  EXPECT_FALSE(device_->GetAccountKey().Ok());
}

TEST_F(PairerBrokerImplTest, SuccessInitialPairingWithDevice) {
  bool triggered_keybase_value_change = false;
  bool triggered_passkey_value_change = false;
  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);
  ConfigurePairingContext();
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairInitialPairing);
  SetUpFastPairRepository();
  SetupProviderGattServer(
      [&]() {
        triggered_keybase_value_change = true;
        EXPECT_OK(TriggerKeyBasedGattChanged());
      },
      [&]() {
        triggered_passkey_value_change = true;
        EXPECT_OK(TriggerPasskeyGattChanged(kPasskey, kProviderPasskeyType));
      });
  CreateFastPairHandshakeInstanceForDevice();
  SetPairingResult(std::nullopt);

  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_);
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  pairing_completed_latch.Await();
  account_key_writed_latch.Await();
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_TRUE(triggered_keybase_value_change);
  EXPECT_TRUE(triggered_passkey_value_change);
  EXPECT_TRUE(device_->GetAccountKey().Ok());

  pairer_broker_->StopPairing();
  EXPECT_FALSE(pairer_broker_->IsPairing());
}

TEST_F(PairerBrokerImplTest, SuccessSubsequentPairingWithDevice) {
  bool triggered_keybase_value_change = false;
  bool triggered_passkey_value_change = false;
  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);
  ConfigurePairingContext();
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairSubsequentPairing);
  SetUpFastPairRepository();
  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_);
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  SetupProviderGattServer(
      [&]() {
        triggered_keybase_value_change = true;
        EXPECT_OK(TriggerKeyBasedGattChanged());
      },
      [&]() {
        triggered_passkey_value_change = true;
        EXPECT_OK(TriggerPasskeyGattChanged(kPasskey, kProviderPasskeyType));
      });
  SetPairingResult(std::nullopt);

  pairer_broker_->PairDevice(*device_);
  // TODO(qinwangz): The logic in `pairer_broker_->IsPairing()` may return false
  // after a call to `PairDevice()`. Please fix.
  // executor_.Execute([&]() {
  //   // Test pairing duplicate devices.
  //   EXPECT_TRUE(pairer_broker_->IsPairing());
  //   pairer_broker_->PairDevice(*device_);
  // });
  device_paired_latch.Await();
  pairing_completed_latch.Await();
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_TRUE(pairer_broker_observer.handshake_completed_);
  EXPECT_TRUE(triggered_keybase_value_change);
  EXPECT_TRUE(triggered_passkey_value_change);
}

TEST_F(PairerBrokerImplTest, SuccessRetroactivePairingWithDevice) {
  ConfigurePairingContext();
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairRetroactivePairing);
  bool triggered_keybase_value_change = false;
  bool triggered_passkey_value_change = false;
  SetUpFastPairRepository();
  SetupProviderGattServer(
      [&]() {
        triggered_keybase_value_change = true;
        EXPECT_OK(TriggerKeyBasedGattChanged());
      },
      [&]() {
        triggered_passkey_value_change = true;
        EXPECT_OK(TriggerPasskeyGattChanged(kPasskey, kProviderPasskeyType));
      });
  CreateFastPairHandshakeInstanceForDevice();
  SetPairingResult(std::nullopt);
  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);
  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_);
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);
  EXPECT_FALSE(device_paired_latch.Await(kWaitTimeout).result());
  pairing_completed_latch.Await();
  account_key_writed_latch.Await();
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_TRUE(triggered_keybase_value_change);
  EXPECT_FALSE(triggered_passkey_value_change);
  EXPECT_TRUE(device_->GetAccountKey().Ok());
}

TEST_F(PairerBrokerImplTest, FaileToCreateHandshakeRetryThreeTimes) {
  bool triggered_keybase_value_change = false;
  bool triggered_passkey_value_change = false;
  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);
  ConfigurePairingContext();
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairSubsequentPairing);
  SetUpFastPairRepository();
  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_);
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  SetupProviderGattServer(
      [&]() { triggered_keybase_value_change = true; },
      [&]() {
        triggered_passkey_value_change = true;
        EXPECT_OK(TriggerPasskeyGattChanged(kPasskey, kProviderPasskeyType));
      });
  SetPairingResult(std::nullopt);

  pairer_broker_->PairDevice(*device_);
  EXPECT_FALSE(device_paired_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_completed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  pairing_failure_latch.Await();
  EXPECT_FALSE(pairer_broker_observer.handshake_completed_);
  EXPECT_TRUE(triggered_keybase_value_change);
  EXPECT_FALSE(triggered_passkey_value_change);
  EXPECT_EQ(pairer_broker_observer.pair_failure_,
            PairFailure::kKeyBasedPairingResponseTimeout);
}

TEST_F(PairerBrokerImplTest, FaileToWriteAccountkey) {
  bool triggered_keybase_value_change = false;
  bool triggered_passkey_value_change = false;
  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);
  ConfigurePairingContext();
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairInitialPairing);
  SetUpFastPairRepository();
  SetupProviderGattServer(
      [&]() {
        triggered_keybase_value_change = true;
        EXPECT_OK(TriggerKeyBasedGattChanged());
      },
      [&]() {
        triggered_passkey_value_change = true;
        EXPECT_OK(TriggerPasskeyGattChanged(kPasskey, kProviderPasskeyType));
      });
  CreateFastPairHandshakeInstanceForDevice();
  SetPairingResult(std::nullopt);
  SetAccountkeyCharacteristicsWriteResultToFailure();

  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_);
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  EXPECT_FALSE(pairing_completed_latch.Await(kWaitTimeout).result());
  account_key_writed_latch.Await();
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_TRUE(triggered_keybase_value_change);
  EXPECT_TRUE(triggered_passkey_value_change);
  EXPECT_FALSE(device_->GetAccountKey().Ok());
  EXPECT_EQ(pairer_broker_observer.account_key_failure_,
            PairFailure::kAccountKeyCharacteristicWrite);
}

TEST_F(PairerBrokerImplTest, FailToPairRetryThreeTimes) {
  bool triggered_keybase_value_change = false;
  bool triggered_passkey_value_change = false;
  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);
  ConfigurePairingContext();
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairInitialPairing);
  SetUpFastPairRepository();
  SetupProviderGattServer(
      [&]() {
        triggered_keybase_value_change = true;
        EXPECT_OK(TriggerKeyBasedGattChanged());
      },
      [&]() {
        triggered_passkey_value_change = true;
        EXPECT_OK(TriggerPasskeyGattChanged("654321", kProviderPasskeyType));
      });
  CreateFastPairHandshakeInstanceForDevice();
  SetPairingResult(std::nullopt);

  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_);
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  EXPECT_FALSE(device_paired_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_completed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  pairing_failure_latch.Await();
  EXPECT_TRUE(triggered_keybase_value_change);
  EXPECT_TRUE(triggered_passkey_value_change);
  EXPECT_FALSE(device_->GetAccountKey().Ok());
  EXPECT_EQ(pairer_broker_observer.pair_failure_,
            PairFailure::kPairingAndConnect);
}
}  // namespace fastpair
}  // namespace nearby
