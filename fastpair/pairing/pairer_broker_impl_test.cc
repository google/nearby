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

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "fastpair//handshake/fast_pair_handshake_lookup.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/crypto/decrypted_passkey.h"
#include "fastpair/crypto/decrypted_response.h"
#include "fastpair/crypto/fast_pair_message_type.h"
#include "fastpair/handshake/fake_fast_pair_data_encryptor.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"
#include "fastpair/handshake/fast_pair_data_encryptor_impl.h"
#include "fastpair/handshake/fast_pair_handshake_impl.h"
#include "fastpair/handshake/fast_pair_handshake_lookup.h"
#include "fastpair/internal/mediums/mediums.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/fake_fast_pair_repository.h"
#include "internal/base/bluetooth_address.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"
#include "internal/test/fake_account_manager.h"

namespace nearby {
namespace fastpair {
namespace {
using Property = nearby::api::ble_v2::GattCharacteristic::Property;
using Permission = nearby::api::ble_v2::GattCharacteristic::Permission;
using ::nearby::api::ble_v2::GattCharacteristic;
using DiscoveryCallback = BluetoothClassicMedium::DiscoveryCallback;

constexpr absl::string_view kFastPairPreferencesFilePath =
    "Google/Nearby/FastPair";
constexpr absl::string_view kTestAccountId = "test_account_id";
constexpr absl::string_view kMetadataId("718c17");
constexpr absl::string_view kPublicAntiSpoof =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
constexpr Uuid kFastPairServiceUuid(0x0000FE2C00001000, 0x800000805F9B34FB);
constexpr Uuid kKeyBasedCharacteristicUuidV2(0xFE2C123483664814,
                                             0x8EB001DE32100BEA);
constexpr Uuid kPasskeyCharacteristicUuidV2(0xFE2C123583664814,
                                            0x8EB001DE32100BEA);
constexpr Uuid kAccountKeyCharacteristicUuidV2(0xFE2C123683664814,
                                               0x8EB001DE32100BEA);
constexpr absl::string_view kKeyBasedResponse("keybasedresponse");
constexpr absl::string_view kPasskeyResponse("passkeyresponse");
constexpr absl::string_view kPasskey("123456");
constexpr std::array<uint8_t, 9> salt = {0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                         0x0D, 0x0E, 0x0F, 0x00};
constexpr absl::Duration kWaitTimeout = absl::Milliseconds(200);
}  // namespace

class FastPairFakeDataEncryptorImplFactory
    : public FastPairDataEncryptorImpl::Factory {
 public:
  void CreateInstance(
      const FastPairDevice& device,
      absl::AnyInvocable<void(std::unique_ptr<FastPairDataEncryptor>)>
          on_get_instance_callback) override {
    if (!successful_retrieval_) {
      std::move(on_get_instance_callback)(nullptr);
      return;
    }

    auto data_encryptor = std::make_unique<FakeFastPairDataEncryptor>();
    data_encryptor_ = data_encryptor.get();
    data_encryptor->SetResponse(response_);
    data_encryptor->SetPasskey(passkey_);
    std::move(on_get_instance_callback)(std::move(data_encryptor));
  }

  FakeFastPairDataEncryptor* data_encryptor() { return data_encryptor_; }

  void SetFailedRetrieval() { successful_retrieval_ = false; }

  void SetResponse(std::optional<DecryptedResponse> response) {
    response_ = std::move(response);
  }

  void SetPasskey(std::optional<DecryptedPasskey> passkey) {
    passkey_ = std::move(passkey);
  }

 private:
  FakeFastPairDataEncryptor* data_encryptor_ = nullptr;
  bool successful_retrieval_ = true;
  std::optional<DecryptedResponse> response_;
  std::optional<DecryptedPasskey> passkey_;
};

struct CharacteristicData {
  // Write result returned to the gatt client.
  absl::Status write_result;
  std::optional<std::string> notify_response;
};

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
  PairerBrokerImplTest() {
    FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
        &fake_data_encryptor_factory_);
  }

  void SetUp() override {
    env_.Start();
    // Setups seeker device.
    mediums_ = std::make_unique<Mediums>();
    account_manager_ = std::make_unique<FakeAccountManager>();

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
    account_manager_.reset();
    device_.reset();
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

  void LogInAccount() {
    AccountManager::Account account;
    account.id = kTestAccountId;
    account_manager_->SetAccount(account);
  }

  void CreateMockDevice(DeviceFastPairVersion version, Protocol protocol) {
    device_ = std::make_unique<FastPairDevice>(
        kMetadataId, remote_device_->GetMacAddress(), protocol);
    proto::GetObservedDeviceResponse response;
    auto metadata = response.mutable_device();
    if (version == DeviceFastPairVersion::kHigherThanV1) {
      std::string decoded_key;
      absl::Base64Unescape(kPublicAntiSpoof, &decoded_key);
      CHECK_EQ(decoded_key.length(), kPublicKeyByteSize);
      metadata->mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
    }
    device_->SetMetadata(DeviceMetadata(response));
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
          latch.CountDown();
        },
        &executor_));
    latch.Await();
    EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(device_.get()));
  }

  std::unique_ptr<FastPairHandshake> CreateConnectedHandshake(
      FastPairDevice& device, Mediums& mediums,
      FastPairHandshake::OnCompleteCallback callback) {
    CountDownLatch latch(1);
    auto handshake = std::make_unique<FastPairHandshakeImpl>(
        device, mediums,
        [&](FastPairDevice& callback_device,
            std::optional<PairFailure> failure) {
          EXPECT_EQ(device_.get(), &callback_device);
          EXPECT_EQ(failure, std::nullopt);
          callback(callback_device, failure);
          latch.CountDown();
        },
        &executor_);
    latch.Await();
    EXPECT_TRUE(handshake->completed_successfully());
    EXPECT_EQ(
        device_->GetPublicAddress().value(),
        device::CanonicalizeBluetoothAddress(remote_device_->GetMacAddress()));
    return handshake;
  }

  // Sets upprovider's gatt_server.
  void SetupProviderGattServer() {
    ble_provider_ = std::make_unique<BleV2Medium>(*adapter_provider_);
    gatt_server_ = ble_provider_->StartGattServer(
        /*ServerGattConnectionCallback=*/{
            .on_characteristic_write_cb =
                [&](const api::ble_v2::BlePeripheral& remote_device,
                    const api::ble_v2::GattCharacteristic& characteristic,
                    int offset, absl::string_view data,
                    BleV2Medium::ServerGattConnectionCallback::
                        WriteValueCallback callback) {
                  MutexLock lock(&mutex_);
                  auto it = characteristics_.find(characteristic);
                  if (it == characteristics_.end()) {
                    callback(absl::NotFoundError("characteristic not found"));
                    return;
                  }
                  callback(it->second.write_result);
                  if (it->second.notify_response.has_value()) {
                    auto ignored = gatt_server_->NotifyCharacteristicChanged(
                        characteristic, false,
                        ByteArray(*it->second.notify_response));
                  }
                }});

    // Insert fast pair related gatt characteristics
    MutexLock lock(&mutex_);
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

  bool SetPairingResult(
      std::optional<api::BluetoothPairingCallback::PairingError> error) {
    return env_.SetPairingResult(&remote_device_->GetImpl(), error);
  }

  void SetNotifyResponse(GattCharacteristic characteristic,
                         absl::string_view response) {
    MutexLock lock(&mutex_);
    CHECK(characteristics_.find(characteristic) != characteristics_.end());
    characteristics_[characteristic].notify_response = response;
  }

  void SetDecryptedResponse() {
    std::array<uint8_t, 6> address_bytes;
    device::ParseBluetoothAddress(
        device::CanonicalizeBluetoothAddress(remote_device_->GetMacAddress()),
        absl::MakeSpan(address_bytes.data(), address_bytes.size()));
    DecryptedResponse decrypted_response(
        FastPairMessageType::kKeyBasedPairingResponse, address_bytes, salt);
    fake_data_encryptor_factory_.SetResponse(std::move(decrypted_response));
  }

  void SetDecryptedPasskey(absl::string_view passkey = kPasskey,
                           FastPairMessageType message_type =
                               FastPairMessageType::kProvidersPasskey) {
    // Random salt
    std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                    0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};

    DecryptedPasskey decrypted_passkey(message_type,
                                       std::stoi(std::string(passkey)), salt);
    fake_data_encryptor_factory_.SetPasskey(std::move(decrypted_passkey));
  }

  void SetPasskeyCharacteristicsWriteResultToFailure() {
    MutexLock lock(&mutex_);
    auto it = characteristics_.find(*passkey_characteristic_);
    it->second.write_result = absl::UnknownError("Failed to write account key");
  }

  void SetAccountkeyCharacteristicsWriteResultToFailure() {
    MutexLock lock(&mutex_);
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
  std::unique_ptr<FakeAccountManager> account_manager_;
  std::unique_ptr<PairerBrokerImpl> pairer_broker_;
  std::optional<GattCharacteristic> key_based_characteristic_;
  std::optional<GattCharacteristic> passkey_characteristic_;
  std::optional<GattCharacteristic> accountkey_characteristic_;
  FastPairFakeDataEncryptorImplFactory fake_data_encryptor_factory_;

 private:
  MediumEnvironment& env_{MediumEnvironment::Instance()};
  Mutex mutex_;
  std::unique_ptr<BluetoothClassicMedium> bt_provider_;
  std::unique_ptr<BluetoothAdapter> adapter_provider_;
  std::unique_ptr<GattServer> gatt_server_;
  std::unique_ptr<BleV2Medium> ble_provider_;
  absl::flat_hash_map<GattCharacteristic, CharacteristicData> characteristics_
      ABSL_GUARDED_BY(mutex_);
  Property properties_ = Property::kWrite | Property::kNotify;
  Permission permissions_ = Permission::kWrite;
};

TEST_F(PairerBrokerImplTest, SuccessInitialPairingWithDeviceV1) {
  LogInAccount();
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kV1,
                   Protocol::kFastPairInitialPairing);
  SetupProviderGattServer();

  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);
  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  pairing_completed_latch.Await();
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
}

TEST_F(PairerBrokerImplTest, SuccessInitialPairingWithDevice) {
  LogInAccount();
  auto repository = std::make_unique<FakeFastPairRepository>();
  repository->SetResultOfIsDeviceSavedToAccount(
      absl::NotFoundError("not found"));
  repository->SetResultOfWriteAccountAssociationToFootprints(absl::OkStatus());
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairInitialPairing);
  SetupProviderGattServer();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  SetNotifyResponse(*passkey_characteristic_, kPasskeyResponse);
  SetDecryptedResponse();
  SetDecryptedPasskey();
  CreateFastPairHandshakeInstanceForDevice();

  EXPECT_FALSE(device_->GetAccountKey().Ok());

  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  pairing_completed_latch.Await();
  account_key_writed_latch.Await();
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_TRUE(device_->GetAccountKey().Ok());

  pairer_broker_->StopPairing();
  EXPECT_FALSE(pairer_broker_->IsPairing());
}

TEST_F(PairerBrokerImplTest, SuccessSubsequentPairingWithDevice) {
  LogInAccount();
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairSubsequentPairing);
  SetupProviderGattServer();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  SetNotifyResponse(*passkey_characteristic_, kPasskeyResponse);
  SetDecryptedResponse();
  SetDecryptedPasskey();

  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  pairing_completed_latch.Await();
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_TRUE(pairer_broker_observer.handshake_completed_);
}

TEST_F(PairerBrokerImplTest, SuccessRetroactivePairingWithDevice) {
  LogInAccount();
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairRetroactivePairing);
  SetupProviderGattServer();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  SetDecryptedResponse();
  CreateFastPairHandshakeInstanceForDevice();
  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);

  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);
  EXPECT_FALSE(device_paired_latch.Await(kWaitTimeout).result());
  pairing_completed_latch.Await();
  account_key_writed_latch.Await();
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_TRUE(device_->GetAccountKey().Ok());
}

TEST_F(PairerBrokerImplTest, FaileToCreateHandshakeRetryThreeTimes) {
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairSubsequentPairing);
  SetupProviderGattServer();

  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  EXPECT_FALSE(device_paired_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_completed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  pairing_failure_latch.Await();
  EXPECT_FALSE(pairer_broker_observer.handshake_completed_);

  EXPECT_EQ(pairer_broker_observer.pair_failure_,
            PairFailure::kKeyBasedPairingResponseTimeout);
}

TEST_F(PairerBrokerImplTest, SkipWriteAccountKeyBecauseNoLoggedInUser) {
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairInitialPairing);
  SetupProviderGattServer();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  SetNotifyResponse(*passkey_characteristic_, kPasskeyResponse);
  SetDecryptedResponse();
  SetDecryptedPasskey();
  CreateFastPairHandshakeInstanceForDevice();

  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);

  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  pairing_completed_latch.Await();
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(device_->GetAccountKey().Ok());
}

TEST_F(PairerBrokerImplTest,
       SkipWriteAccountKeyBecauseDeviceAlreadySavedToAccount) {
  LogInAccount();
  auto repository = std::make_unique<FakeFastPairRepository>();
  repository->SetResultOfIsDeviceSavedToAccount(absl::OkStatus());
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairInitialPairing);
  SetupProviderGattServer();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  SetNotifyResponse(*passkey_characteristic_, kPasskeyResponse);
  SetDecryptedResponse();
  SetDecryptedPasskey();
  CreateFastPairHandshakeInstanceForDevice();

  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  pairing_completed_latch.Await();
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
}

TEST_F(PairerBrokerImplTest, FaileToWriteAccountkeyToRemoteDevice) {
  LogInAccount();
  auto repository = std::make_unique<FakeFastPairRepository>();
  repository->SetResultOfIsDeviceSavedToAccount(
      absl::NotFoundError("not found"));
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairInitialPairing);
  SetupProviderGattServer();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  SetNotifyResponse(*passkey_characteristic_, kPasskeyResponse);
  SetDecryptedResponse();
  SetDecryptedPasskey();
  CreateFastPairHandshakeInstanceForDevice();

  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);
  SetAccountkeyCharacteristicsWriteResultToFailure();

  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  EXPECT_FALSE(pairing_completed_latch.Await(kWaitTimeout).result());
  account_key_writed_latch.Await();
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(device_->GetAccountKey().Ok());
  EXPECT_EQ(pairer_broker_observer.account_key_failure_,
            PairFailure::kAccountKeyCharacteristicWrite);
}

TEST_F(PairerBrokerImplTest, FaileToWriteAccountkeyToFootprints) {
  LogInAccount();
  auto repository = std::make_unique<FakeFastPairRepository>();
  repository->SetResultOfIsDeviceSavedToAccount(
      absl::NotFoundError("not found"));
  repository->SetResultOfWriteAccountAssociationToFootprints(
      absl::InternalError("Failed to write account key to foot prints"));
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairInitialPairing);
  SetupProviderGattServer();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  SetNotifyResponse(*passkey_characteristic_, kPasskeyResponse);
  SetDecryptedResponse();
  SetDecryptedPasskey();
  CreateFastPairHandshakeInstanceForDevice();

  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);

  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  device_paired_latch.Await();
  EXPECT_FALSE(pairing_completed_latch.Await(kWaitTimeout).result());
  account_key_writed_latch.Await();
  EXPECT_FALSE(pairing_failure_latch.Await(kWaitTimeout).result());
  EXPECT_TRUE(device_->GetAccountKey().Ok());
  EXPECT_EQ(pairer_broker_observer.account_key_failure_,
            PairFailure::kWriteAccountKeyToFootprints);
}

TEST_F(PairerBrokerImplTest, FailToPairRetryThreeTimes) {
  LogInAccount();
  ConfigurePairingContext();
  SetPairingResult(std::nullopt);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   Protocol::kFastPairInitialPairing);
  SetupProviderGattServer();
  SetNotifyResponse(*key_based_characteristic_, kKeyBasedResponse);
  SetNotifyResponse(*passkey_characteristic_, kPasskeyResponse);
  SetDecryptedResponse();
  SetDecryptedPasskey("654321");
  CreateFastPairHandshakeInstanceForDevice();

  CountDownLatch device_paired_latch(1);
  CountDownLatch account_key_writed_latch(1);
  CountDownLatch pairing_completed_latch(1);
  CountDownLatch pairing_failure_latch(1);

  EXPECT_FALSE(device_->GetAccountKey().Ok());

  pairer_broker_ = std::make_unique<PairerBrokerImpl>(*mediums_, &executor_,
                                                      account_manager_.get());
  PairerBrokerObserver pairer_broker_observer(
      pairer_broker_.get(), &device_paired_latch, &account_key_writed_latch,
      &pairing_completed_latch, &pairing_failure_latch);
  pairer_broker_->PairDevice(*device_);

  EXPECT_FALSE(device_paired_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(pairing_completed_latch.Await(kWaitTimeout).result());
  EXPECT_FALSE(account_key_writed_latch.Await(kWaitTimeout).result());
  pairing_failure_latch.Await();
  EXPECT_FALSE(device_->GetAccountKey().Ok());
  EXPECT_EQ(pairer_broker_observer.pair_failure_,
            PairFailure::kPairingAndConnect);
}
}  // namespace fastpair
}  // namespace nearby
