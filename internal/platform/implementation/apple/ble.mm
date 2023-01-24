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

#import "internal/platform/implementation/apple/ble.h"

#include <CoreBluetooth/CoreBluetooth.h>

#include <string>
#include <utility>

#include "internal/platform/implementation/apple/bluetooth_adapter.h"
#include "internal/platform/implementation/apple/utils.h"
#include "internal/platform/implementation/ble_v2.h"

#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleCentral.h"
#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBlePeripheral.h"

namespace nearby {
namespace apple {

namespace {

CBAttributePermissions PermissionToCBPermissions(
    const std::vector<api::ble_v2::GattCharacteristic::Permission>& permissions) {
  CBAttributePermissions characteristPermissions = 0;
  for (const auto& permission : permissions) {
    switch (permission) {
      case api::ble_v2::GattCharacteristic::Permission::kRead:
        characteristPermissions |= CBAttributePermissionsReadable;
        break;
      case api::ble_v2::GattCharacteristic::Permission::kWrite:
        characteristPermissions |= CBAttributePermissionsWriteable;
        break;
      case api::ble_v2::GattCharacteristic::Permission::kLast:
      case api::ble_v2::GattCharacteristic::Permission::kUnknown:
      default:;  // fall through
    }
  }
  return characteristPermissions;
}

CBCharacteristicProperties PropertiesToCBProperties(
    const std::vector<api::ble_v2::GattCharacteristic::Property>& properties) {
  CBCharacteristicProperties characteristicProperties = 0;
  for (const auto& property : properties) {
    switch (property) {
      case api::ble_v2::GattCharacteristic::Property::kRead:
        characteristicProperties |= CBCharacteristicPropertyRead;
        break;
      case api::ble_v2::GattCharacteristic::Property::kWrite:
        characteristicProperties |= CBCharacteristicPropertyWrite;
        break;
      case api::ble_v2::GattCharacteristic::Property::kIndicate:
        characteristicProperties |= CBCharacteristicPropertyIndicate;
        break;
      case api::ble_v2::GattCharacteristic::Property::kLast:
      case api::ble_v2::GattCharacteristic::Property::kUnknown:
      default:;  // fall through
    }
  }
  return characteristicProperties;
}

}  // namespace

using ::nearby::api::ble_v2::BleAdvertisementData;
using ::nearby::api::ble_v2::TxPowerLevel;
using ScanCallback = ::nearby::api::ble_v2::BleMedium::ScanCallback;

/** InputStream that reads from GNCMConnection. */
BleInputStream::BleInputStream()
    : newDataPackets_([NSMutableArray array]),
      accumulatedData_([NSMutableData data]),
      condition_([[NSCondition alloc] init]) {
  // Create the handlers of incoming data from the remote endpoint.
  connectionHandlers_ = [GNCMConnectionHandlers
      payloadHandler:^(NSData* data) {
        [condition_ lock];
        // Add the incoming data to the data packet array to be processed in read() below.
        [newDataPackets_ addObject:data];
        [condition_ signal];
        [condition_ unlock];
      }
      disconnectedHandler:^{
        [condition_ lock];
        // Release the data packet array, meaning the stream has been closed or severed.
        newDataPackets_ = nil;
        [condition_ signal];
        [condition_ unlock];
      }];
}

BleInputStream::~BleInputStream() {
  NSCAssert(!newDataPackets_, @"BleInputStream not closed before destruction");
}

ExceptionOr<ByteArray> BleInputStream::Read(std::int64_t size) {
  // Block until either (a) the connection has been closed, (b) we have enough data to return.
  NSData* dataToReturn;
  [condition_ lock];
  while (true) {
    // Check if the stream has been closed or severed.
    if (!newDataPackets_) break;

    if (newDataPackets_.count > 0) {
      // Add the packet data to the accumulated data.
      for (NSData* data in newDataPackets_) {
        if (data.length > 0) {
          [accumulatedData_ appendData:data];
        }
      }
      [newDataPackets_ removeAllObjects];
    }

    if ((size == -1) && (accumulatedData_.length > 0)) {
      // Return all of the data.
      dataToReturn = accumulatedData_;
      accumulatedData_ = [NSMutableData data];
      break;
    } else if (accumulatedData_.length > 0) {
      // Return up to |size| bytes of the data.
      std::int64_t sizeToReturn = (accumulatedData_.length < size) ? accumulatedData_.length : size;
      NSRange range = NSMakeRange(0, (NSUInteger)sizeToReturn);
      dataToReturn = [accumulatedData_ subdataWithRange:range];
      [accumulatedData_ replaceBytesInRange:range withBytes:nil length:0];
      break;
    }

    [condition_ wait];
  }
  [condition_ unlock];

  if (dataToReturn) {
    NSLog(@"[NEARBY] Input stream: Received data of size: %lu", (unsigned long)dataToReturn.length);
    return ExceptionOr<ByteArray>(ByteArrayFromNSData(dataToReturn));
  } else {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }
}

Exception BleInputStream::Close() {
  // Unblock pending read operation.
  [condition_ lock];
  newDataPackets_ = nil;
  [condition_ signal];
  [condition_ unlock];
  return {Exception::kSuccess};
}

/** OutputStream that writes to GNCMConnection. */
BleOutputStream::~BleOutputStream() {
  NSCAssert(!connection_, @"BleOutputStream not closed before destruction");
}

Exception BleOutputStream::Write(const ByteArray& data) {
  [condition_ lock];
  NSLog(@"[NEARBY] Sending data of size: %lu", (unsigned long)NSDataFromByteArray(data).length);

  NSMutableData* packet = [NSMutableData dataWithData:NSDataFromByteArray(data)];

  // Send the data, blocking until the completion handler is called.
  __block GNCMPayloadResult sendResult = GNCMPayloadResultFailure;
  __block bool isComplete = NO;
  NSCondition* condition = condition_;  // don't capture |this| in completion

  // Check if connection_ is nil, then just don't wait and return as failure.
  if (connection_ != nil) {
    [connection_ sendData:packet
        progressHandler:^(size_t count) {
        }
        completion:^(GNCMPayloadResult result) {
          // Make sure we haven't already reported completion before. This prevents a crash
          // where we try leaving a dispatch group more times than we entered it.
          // b/79095653.
          if (isComplete) {
            return;
          }
          isComplete = YES;
          sendResult = result;
          [condition lock];
          [condition signal];
          [condition unlock];
        }];
    [condition_ wait];
    [condition_ unlock];
  } else {
    sendResult = GNCMPayloadResultFailure;
    [condition_ unlock];
  }

  if (sendResult == GNCMPayloadResultSuccess) {
    return {Exception::kSuccess};
  } else {
    return {Exception::kIo};
  }
}

Exception BleOutputStream::Flush() {
  // The write() function blocks until the data is received by the remote endpoint, so there's
  // nothing to do here.
  return {Exception::kSuccess};
}

Exception BleOutputStream::Close() {
  // Unblock pending write operation.
  [condition_ lock];
  connection_ = nil;
  [condition_ signal];
  [condition_ unlock];
  return {Exception::kSuccess};
}

/** BleSocket implementation.*/
BleSocket::BleSocket(id<GNCMConnection> connection, BlePeripheral* peripheral)
    : input_stream_(new BleInputStream()),
      output_stream_(new BleOutputStream(connection)),
      peripheral_(peripheral) {}

BleSocket::~BleSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

bool BleSocket::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

Exception BleSocket::Close() {
  absl::MutexLock lock(&mutex_);
  DoClose();
  return {Exception::kSuccess};
}

void BleSocket::DoClose() {
  if (!closed_) {
    input_stream_->Close();
    output_stream_->Close();
    closed_ = true;
  }
}

/** WifiLanServerSocket implementation. */
BleServerSocket::~BleServerSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

std::unique_ptr<api::ble_v2::BleSocket> BleServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  // Return early if closed.
  if (closed_) return {};

  auto remote_socket = std::move(pending_sockets_.extract(pending_sockets_.begin()).value());
  return std::move(remote_socket);
}

bool BleServerSocket::Connect(std::unique_ptr<BleSocket> socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return false;
  }
  // add client socket to the pending list
  pending_sockets_.insert(std::move(socket));
  cond_.SignalAll();
  if (closed_) {
    return false;
  }
  return true;
}

void BleServerSocket::SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

Exception BleServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception BleServerSocket::DoClose() {
  bool should_notify = !closed_;
  closed_ = true;
  if (should_notify) {
    cond_.SignalAll();
    if (close_notifier_) {
      auto notifier = std::move(close_notifier_);
      mutex_.Unlock();
      // Notifier may contain calls to public API, and may cause deadlock, if
      // mutex_ is held during the call.
      notifier();
      mutex_.Lock();
    }
  }
  return {Exception::kSuccess};
}

/** BleMedium implementation. */
BleMedium::BleMedium(::nearby::api::BluetoothAdapter& adapter)
    : adapter_(static_cast<BluetoothAdapter*>(&adapter)) {}

bool BleMedium::StartAdvertising(
    const BleAdvertisementData& advertising_data,
    ::nearby::api::ble_v2::AdvertiseParameters advertise_set_parameters) {
  if (advertising_data.service_data.empty()) {
    return false;
  }
  const auto& service_uuid = advertising_data.service_data.begin()->first.Get16BitAsString();
  const ByteArray& service_data_bytes = advertising_data.service_data.begin()->second;

  if (!peripheral_) {
    peripheral_ = [[GNCMBlePeripheral alloc] init];
  }

  auto& peripheral = adapter_->GetPeripheral();
  [peripheral_
      startAdvertisingWithServiceUUID:ObjCStringFromCppString(service_uuid)
                    advertisementData:NSDataFromByteArray(service_data_bytes)
             endpointConnectedHandler:^GNCMConnectionHandlers*(id<GNCMConnection> connection) {
               // TODO(edwinwu): This server_socket is supposed to be gotten from the map by key of
               // servcie_id. We now always get the first iteration since we don't know the key now.
               // Try the way to move the Ble socket frame verification up to one layer.
               std::string service_id;
               BleServerSocket* server_socket;
               if (!server_sockets_.empty()) {
                 service_id = server_sockets_.begin()->first;
                 server_socket = server_sockets_.begin()->second;
               } else {
                 return nil;
               }
               auto socket = std::make_unique<BleSocket>(connection, &peripheral);
               GNCMConnectionHandlers* connectionHandlers =
                   static_cast<BleInputStream&>(socket->GetInputStream()).GetConnectionHandlers();
               server_socket->Connect(std::move(socket));
               return connectionHandlers;
             }
                        callbackQueue:callback_queue_];
  return true;
}

bool BleMedium::StopAdvertising() {
  peripheral_ = nil;
  return true;
}
std::unique_ptr<BleMedium::AdvertisingSession> BleMedium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData& advertising_data,
    api::ble_v2::AdvertiseParameters advertise_parameters,
    BleMedium::AdvertisingCallback callback) {
  // TODO(hais): add real impl for iOs StartAdvertising
  return std::make_unique<AdvertisingSession>(AdvertisingSession{});
}
bool BleMedium::StartScanning(const Uuid& service_uuid, TxPowerLevel tx_power_level,
                              ScanCallback scan_callback) {
  if (!central_) {
    central_ = [[GNCMBleCentral alloc] init];
  }

  __block ScanCallback callback = std::move(scan_callback);
  [central_ startScanningWithServiceUUID:ObjCStringFromCppString(service_uuid.Get16BitAsString())
      scanResultHandler:^(NSString* peripheralID, NSData* serviceData) {
        BleAdvertisementData advertisement_data;
        advertisement_data.service_data = {{service_uuid, ByteArrayFromNSData(serviceData)}};
        BlePeripheral& peripheral = adapter_->GetPeripheral();
        peripheral.SetPeripheralId(CppStringFromObjCString(peripheralID));
        callback.advertisement_found_cb(peripheral, advertisement_data);
      }
      requestConnectionHandler:^(GNCMBleConnectionRequester connectionRequester) {
        BlePeripheral& peripheral = adapter_->GetPeripheral();
        peripheral.SetConnectionRequester(connectionRequester);
      }
      callbackQueue:callback_queue_];

  return true;
}

bool BleMedium::StopScanning() {
  central_ = nil;
  return true;
}

std::unique_ptr<BleMedium::ScanningSession> BleMedium::StartScanning(
    const Uuid& service_uuid, TxPowerLevel tx_power_level, BleMedium::ScanningCallback callback) {
  // TODO(hais): add real impl for windows StartScanning.
  return std::make_unique<ScanningSession>(ScanningSession{});
}

std::unique_ptr<api::ble_v2::GattServer> BleMedium::StartGattServer(
    api::ble_v2::ServerGattConnectionCallback callback) {
  if (!peripheral_) {
    peripheral_ = [[GNCMBlePeripheral alloc] init];
  }
  return std::make_unique<GattServer>(peripheral_);
}

std::unique_ptr<api::ble_v2::GattClient> BleMedium::ConnectToGattServer(
    api::ble_v2::BlePeripheral& peripheral, TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError* connectedError;
  BlePeripheral iosPeripheral = static_cast<BlePeripheral&>(peripheral);
  std::string peripheral_id = iosPeripheral.GetPeripheralId();
  [central_ connectGattServerWithPeripheralID:ObjCStringFromCppString(peripheral_id)
                  gattConnectionResultHandler:^(NSError* _Nullable error) {
                    connectedError = error;
                    dispatch_semaphore_signal(semaphore);
                  }];
  dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));

  if (connectedError) {
    return nullptr;
  }
  return std::make_unique<GattClient>(central_, peripheral_id);
}

std::unique_ptr<api::ble_v2::BleServerSocket> BleMedium::OpenServerSocket(
    const std::string& service_id) {
  auto server_socket = std::make_unique<BleServerSocket>();
  server_socket->SetCloseNotifier([this, service_id]() {
    absl::MutexLock lock(&mutex_);
    server_sockets_.erase(service_id);
  });
  absl::MutexLock lock(&mutex_);
  server_sockets_.insert({service_id, server_socket.get()});
  return server_socket;
}

std::unique_ptr<api::ble_v2::BleSocket> BleMedium::Connect(const std::string& service_id,
                                                           TxPowerLevel tx_power_level,
                                                           api::ble_v2::BlePeripheral& peripheral,
                                                           CancellationFlag* cancellation_flag) {
  NSString* serviceID = ObjCStringFromCppString(service_id);
  __block std::unique_ptr<BleSocket> socket;
  __block BlePeripheral ios_peripheral = static_cast<BlePeripheral&>(peripheral);
  GNCMBleConnectionRequester connection_requester = ios_peripheral.GetConnectionRequester();
  if (!connection_requester) return {};

  dispatch_group_t group = dispatch_group_create();
  dispatch_group_enter(group);
  if (cancellation_flag->Cancelled()) {
    NSLog(@"[NEARBY] BLE Connect: Has been cancelled: service_id=%@", serviceID);
    dispatch_group_leave(group);  // unblock
    return {};
  }

  connection_requester(serviceID, ^(id<GNCMConnection> connection) {
    // If the connection wasn't successfully established, return a NULL socket.
    if (connection) {
      socket = std::make_unique<BleSocket>(connection, &ios_peripheral);
    }

    dispatch_group_leave(group);  // unblock
    return socket != nullptr
               ? static_cast<BleInputStream&>(socket->GetInputStream()).GetConnectionHandlers()
               : nullptr;
  });
  dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

  // Send the (empty) intro packet, which the BLE advertiser is expecting.
  if (socket != nullptr) {
    socket->GetOutputStream().Write(ByteArray());
  }

  return std::move(socket);
}

bool BleMedium::IsExtendedAdvertisementsAvailable() { return false; }

// NOLINTNEXTLINE
absl::optional<api::ble_v2::GattCharacteristic> BleMedium::GattServer::CreateCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid,
    const std::vector<api::ble_v2::GattCharacteristic::Permission>& permissions,
    const std::vector<api::ble_v2::GattCharacteristic::Property>& properties) {
  api::ble_v2::GattCharacteristic characteristic = {.uuid = characteristic_uuid,
                                                    .service_uuid = service_uuid,
                                                    .permissions = permissions,
                                                    .properties = properties};
  [peripheral_
      addCBServiceWithUUID:[CBUUID
                               UUIDWithString:ObjCStringFromCppString(
                                                  characteristic.service_uuid.Get16BitAsString())]];
  [peripheral_
      addCharacteristic:[[CBMutableCharacteristic alloc]
                            initWithType:[CBUUID UUIDWithString:ObjCStringFromCppString(std::string(
                                                                    characteristic.uuid))]
                              properties:PropertiesToCBProperties(characteristic.properties)
                                   value:nil
                             permissions:PermissionToCBPermissions(characteristic.permissions)]];
  return characteristic;
}

bool BleMedium::GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic, const nearby::ByteArray& value) {
  [peripheral_ updateValue:NSDataFromByteArray(value)
         forCharacteristic:[CBUUID UUIDWithString:ObjCStringFromCppString(
                                                      std::string(characteristic.uuid))]];
  return true;
}

void BleMedium::GattServer::Stop() { [peripheral_ stopGATTService]; }

bool BleMedium::GattClient::DiscoverServiceAndCharacteristics(
    const Uuid& service_uuid, const std::vector<Uuid>& characteristic_uuids) {
  // Discover all characteristics that may contain the advertisement.
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  gatt_characteristic_values_.clear();
  CBUUID* serviceUUID = [CBUUID UUIDWithString:ObjCStringFromCppString(std::string(service_uuid))];

  absl::flat_hash_map<std::string, Uuid> gatt_characteristics;
  NSMutableArray<CBUUID*>* characteristicUUIDs =
      [NSMutableArray arrayWithCapacity:characteristic_uuids.size()];
  for (const auto& characteristic_uuid : characteristic_uuids) {
    [characteristicUUIDs addObject:[CBUUID UUIDWithString:ObjCStringFromCppString(
                                                              std::string(characteristic_uuid))]];
    gatt_characteristics.insert({std::string(characteristic_uuid), characteristic_uuid});
  }

  [central_ discoverGattService:serviceUUID
            gattCharacteristics:characteristicUUIDs
                   peripheralID:ObjCStringFromCppString(peripheral_id_)
      gattDiscoverResultHandler:^(NSOrderedSet<CBCharacteristic*>* _Nullable cb_characteristics) {
        if (cb_characteristics != nil) {
          for (CBCharacteristic* cb_characteristic in cb_characteristics) {
            auto const& it = gatt_characteristics.find(
                CppStringFromObjCString(cb_characteristic.UUID.UUIDString));
            if (it == gatt_characteristics.end()) continue;

            api::ble_v2::GattCharacteristic characteristic = {.uuid = it->second,
                                                              .service_uuid = service_uuid};
            gatt_characteristic_values_.insert(
                {characteristic, ByteArrayFromNSData(cb_characteristic.value)});
          }
        }

        dispatch_semaphore_signal(semaphore);
      }];

  dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC));

  if (gatt_characteristic_values_.empty()) {
    return false;
  }
  return true;
}

// NOLINTNEXTLINE
absl::optional<api::ble_v2::GattCharacteristic> BleMedium::GattClient::GetCharacteristic(
    const Uuid& service_uuid, const Uuid& characteristic_uuid) {
  api::ble_v2::GattCharacteristic characteristic = {.uuid = characteristic_uuid,
                                                    .service_uuid = service_uuid};
  auto const it = gatt_characteristic_values_.find(characteristic);
  if (it == gatt_characteristic_values_.end()) {
    return absl::nullopt;  // NOLINT
  }
  return it->first;
}

// NOLINTNEXTLINE
absl::optional<ByteArray> BleMedium::GattClient::ReadCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic) {
  auto const it = gatt_characteristic_values_.find(characteristic);
  if (it == gatt_characteristic_values_.end()) {
    return absl::nullopt;  // NOLINT
  }
  return it->second;
}

bool BleMedium::GattClient::WriteCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic, const ByteArray& value) {
  // No op.
  return false;
}

void BleMedium::GattClient::Disconnect() {
  [central_ disconnectGattServiceWithPeripheralID:ObjCStringFromCppString(peripheral_id_)];
}

}  // namespace apple
}  // namespace nearby
