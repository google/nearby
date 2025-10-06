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

#import "internal/platform/implementation/apple/ble_medium.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "internal/platform/implementation/apple/ble_utils.h"
#include "internal/platform/implementation/apple/utils.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/bluetooth_adapter.h"

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPClient.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPConnection.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEL2CAPServer.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEMedium.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
// TODO(b/293336684): Old Weave imports that need to be deleted once shared Weave is complete.
#import "internal/platform/implementation/apple/Mediums/BLE/GNCMBleConnection.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCMBleUtils.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Central/GNSCentralPeerManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralManager.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Peripheral/GNSPeripheralServiceManager.h"
#import "internal/platform/implementation/apple/ble_gatt_client.h"
#import "internal/platform/implementation/apple/ble_gatt_server.h"
#import "internal/platform/implementation/apple/ble_l2cap_server_socket.h"
#import "internal/platform/implementation/apple/ble_l2cap_socket.h"
#import "internal/platform/implementation/apple/ble_server_socket.h"
#import "internal/platform/implementation/apple/ble_socket.h"
#import "internal/platform/implementation/apple/bluetooth_adapter_v2.h"
#import "internal/platform/implementation/apple/utils.h"

static NSString *const kWeaveServiceUUID = @"FEF3";
static const char *const kConnectionCallbackQueueLabel =
    "com.google.nearby.ble_medium.connection_callback_queue";

// Timeout for BLE operations that are initiated by a call to a public API.
static const UInt8 kApiTimeoutInSeconds = 12;
static NSTimeInterval const kThresholdInterval = 2;                                // 2 seconds
static NSTimeInterval const kAdvertisementPacketsMapExpirationTimeInterval = 600;  // 10 minutes

namespace nearby {
namespace apple {
namespace {

NSString *ConvertDataToHexString(NSData *data) {
  NSUInteger dataLength = data.length;
  if (dataLength == 0) {
    return @"0x";
  }

  const unsigned char *dataBuffer = (const unsigned char *)data.bytes;
  NSMutableString *hexString = [NSMutableString stringWithCapacity:dataLength * 2];

  for (NSUInteger i = 0; i < dataLength; ++i) {
    [hexString appendFormat:@"%02lx", (unsigned long)dataBuffer[i]];
  }

  return [NSString stringWithFormat:@"0x%@", hexString];
}

}  // namespace

BleMedium::BleMedium() : medium_([[GNCBLEMedium alloc] init]) {
  connection_callback_queue_ =
      dispatch_queue_create(kConnectionCallbackQueueLabel, DISPATCH_QUEUE_SERIAL);
}

// ble_medium.mm
BleMedium::~BleMedium() { [medium_ stop]; }

std::unique_ptr<api::ble::BleMedium::AdvertisingSession> BleMedium::StartAdvertising(
    const api::ble::BleAdvertisementData &advertising_data,
    api::ble::AdvertiseParameters advertise_set_parameters,
    api::ble::BleMedium::AdvertisingCallback callback) {
  NSMutableDictionary<CBUUID *, NSData *> *serviceData =
      ObjCServiceDataFromCPP(advertising_data.service_data);

  __block api::ble::BleMedium::AdvertisingCallback blockCallback = std::move(callback);

  [socketPeripheralManager_ start];

  [medium_ startAdvertisingData:serviceData
              completionHandler:^(NSError *error) {
                blockCallback.start_advertising_result(
                    error == nil ? absl::OkStatus()
                                 : absl::InternalError(error.localizedDescription.UTF8String));
              }];

  return std::make_unique<AdvertisingSession>(AdvertisingSession{.stop_advertising = [this] {
    return StopAdvertising() ? absl::OkStatus() : absl::InternalError("Failed to stop advertising");
  }});
}

bool BleMedium::StartAdvertising(const api::ble::BleAdvertisementData &advertising_data,
                                 api::ble::AdvertiseParameters advertise_set_parameters) {
  NSMutableDictionary<CBUUID *, NSData *> *serviceData =
      ObjCServiceDataFromCPP(advertising_data.service_data);

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ startAdvertisingData:serviceData
              completionHandler:^(NSError *error) {
                if (error != nil) {
                  GNCLoggerError(@"Failed to start advertising: %@", error);
                  blockError = error;
                }
                dispatch_semaphore_signal(semaphore);
              }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"Start advertising operation timed out.");
    return false;
  }
  return blockError == nil;
}

bool BleMedium::StopAdvertising() {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ stopAdvertisingWithCompletionHandler:^(NSError *error) {
    if (error != nil) {
      GNCLoggerError(@"Failed to stop advertising: %@", error);
      blockError = error;
    }
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"Stop advertising operation timed out.");
    return false;
  }
  return blockError == nil;
}

void BleMedium::HandleAdvertisementFound(id<GNCPeripheral> peripheral,
                                         NSDictionary<CBUUID *, NSData *> *serviceData) {
  api::ble::BleAdvertisementData data;
  NSDate *now = [NSDate date];

  if ([now timeIntervalSinceDate:GetLastTimestampToCleanExpiredAdvertisementPackets()] >
      kAdvertisementPacketsMapExpirationTimeInterval) {
    ClearAdvertisementPacketsMap();
  }

  api::ble::BlePeripheral::UniqueId peripheral_id = peripheral.identifier.hash;

  if (!ShouldReportAdvertisement(now, peripheral_id, serviceData)) {
    return;
  }

  for (CBUUID *key in serviceData.allKeys) {
    data.service_data[CPPUUIDFromObjC(key)] = ByteArrayFromNSData(serviceData[key]);
  }

  // Add the peripheral to the map if we haven't discovered it yet.
  api::ble::BlePeripheral::UniqueId unique_id = peripherals_.Add(peripheral);
  AddAdvertisementPacketInfo(unique_id, serviceData);

#if DEBUG
  for (NSData *service_data in serviceData.allValues) {
    GNCLoggerDebug(@"Reporting the advertisement packet to upper layer for unique_id: %llu, %@, "
                   @"advertisement_data: %@.",
                   unique_id, peripheral, ConvertDataToHexString(service_data));
  }
#endif

  if (scanning_cb_.advertisement_found_cb) {
    scanning_cb_.advertisement_found_cb(unique_id, data);
  }
  if (scan_cb_.advertisement_found_cb) {
    scan_cb_.advertisement_found_cb(unique_id, data);
  }
}

std::unique_ptr<api::ble::BleMedium::ScanningSession> BleMedium::StartScanning(
    const Uuid &service_uuid, api::ble::TxPowerLevel tx_power_level,
    api::ble::BleMedium::ScanningCallback callback) {
  CBUUID *serviceUUID = CBUUID128FromCPP(service_uuid);
  scanning_cb_ = std::move(callback);

  // Clear the map of discovered peripherals only when we are starting a new scan. If we cleared the
  // map every time we stopped a scan, we would not be able to connect to peripherals that we
  // discovered in that scan session.
  peripherals_.Clear();
  ClearAdvertisementPacketsMap();

  socketCentralManager_ = [[GNSCentralManager alloc] initWithSocketServiceUUID:serviceUUID];
  [socketCentralManager_ startNoScanModeWithAdvertisedServiceUUIDs:@[ serviceUUID ]];

  [medium_ startScanningForService:serviceUUID
      advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                  NSDictionary<CBUUID *, NSData *> *serviceData) {
        callback_executor_.Execute(
            [this, peripheral, serviceData] { HandleAdvertisementFound(peripheral, serviceData); });
      }
      completionHandler:^(NSError *error) {
        if (scanning_cb_.start_scanning_result) {
          scanning_cb_.start_scanning_result(
              error == nil ? absl::OkStatus()
                           : absl::InternalError(error.localizedDescription.UTF8String));
        }
      }];

  return std::make_unique<ScanningSession>(ScanningSession{.stop_scanning = [this] {
    return StopScanning() ? absl::OkStatus() : absl::InternalError("Failed to stop scanning");
  }});
}

bool BleMedium::StartScanning(const Uuid &service_uuid, api::ble::TxPowerLevel tx_power_level,
                              api::ble::BleMedium::ScanCallback callback) {
  CBUUID *serviceUUID = CBUUID128FromCPP(service_uuid);
  scan_cb_ = std::move(callback);

  // Clear the map of discovered peripherals only when we are starting a new scan. If we cleared the
  // map every time we stopped a scan, we would not be able to connect to peripherals that we
  // discovered in that scan session.
  peripherals_.Clear();
  ClearAdvertisementPacketsMap();

  socketCentralManager_ = [[GNSCentralManager alloc] initWithSocketServiceUUID:serviceUUID];
  [socketCentralManager_ startNoScanModeWithAdvertisedServiceUUIDs:@[ serviceUUID ]];

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ startScanningForService:serviceUUID
      advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                  NSDictionary<CBUUID *, NSData *> *serviceData) {
        callback_executor_.Execute(
            [this, peripheral, serviceData] { HandleAdvertisementFound(peripheral, serviceData); });
      }
      completionHandler:^(NSError *error) {
        if (error != nil) {
          GNCLoggerError(@"Failed to start scanning: %@", error);
          blockError = error;
        }
        dispatch_semaphore_signal(semaphore);
      }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"Start scanning operation timed out.");
    return false;
  }
  return blockError == nil;
}

bool BleMedium::StartMultipleServicesScanning(const std::vector<Uuid> &service_uuids,
                                              api::ble::TxPowerLevel tx_power_level,
                                              api::ble::BleMedium::ScanCallback callback) {
  if (service_uuids.empty()) {
    GNCLoggerError(@"No service UUIDs provided");
    return false;
  }

  NSMutableArray<CBUUID *> *serviceUUIDs = [NSMutableArray arrayWithCapacity:service_uuids.size()];
  for (const Uuid &service_uuid : service_uuids) {
    [serviceUUIDs addObject:CBUUID128FromCPP(service_uuid)];
  }

  scan_cb_ = std::move(callback);

  // Clear the map of discovered peripherals only when we are starting a new scan. If we cleared the
  // map every time we stopped a scan, we would not be able to connect to peripherals that we
  // discovered in that scan session.
  peripherals_.Clear();
  ClearAdvertisementPacketsMap();

  socketCentralManager_ = [[GNSCentralManager alloc] initWithSocketServiceUUID:serviceUUIDs[0]];
  [socketCentralManager_ startNoScanModeWithAdvertisedServiceUUIDs:@[ serviceUUIDs[0] ]];

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ startScanningForMultipleServices:serviceUUIDs
      advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                  NSDictionary<CBUUID *, NSData *> *serviceData) {
        callback_executor_.Execute(
            [this, peripheral, serviceData] { HandleAdvertisementFound(peripheral, serviceData); });
      }
      completionHandler:^(NSError *error) {
        if (error != nil) {
          GNCLoggerError(@"Failed to start scanning for multiple services: %@", error);
          blockError = error;
        }
        dispatch_semaphore_signal(semaphore);
      }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"Start scanning for multiple services operation timed out.");
    return false;
  }
  return blockError == nil;
}

bool BleMedium::StopScanning() {
  [socketCentralManager_ stopNoScanMode];

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ stopScanningWithCompletionHandler:^(NSError *error) {
    if (error != nil) {
      GNCLoggerError(@"Failed to stop scanning: %@", error);
      blockError = error;
    }
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"Stop scanning operation timed out.");
    return false;
  }
  return blockError == nil;
}

bool BleMedium::PauseMediumScanning() { return StopScanning(); }

bool BleMedium::ResumeMediumScanning() {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ resumeMediumScanning:^(NSError *error) {
    if (error != nil) {
      GNCLoggerError(@"Failed to resume scanning: %@", error);
      blockError = error;
    }
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"ResumeMediumScanning operation timed out.");
    return false;
  }
  return blockError == nil;
}

// TODO(b/290385712): Add implementation that calls ServerGattConnectionCallback methods.
std::unique_ptr<api::ble::GattServer> BleMedium::StartGattServer(
    api::ble::ServerGattConnectionCallback callback) {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block GNCBLEGATTServer *blockServer = nil;
  __block NSError *blockError = nil;
  [medium_ startGATTServerWithCompletionHandler:^(GNCBLEGATTServer *server, NSError *error) {
    if (error != nil) {
      GNCLoggerError(@"Error starting GATT server: %@", error);
      blockError = error;
    } else {
      GNCLoggerInfo(@"Starting GATT server operation completed.");
      blockServer = server;
    }
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"Starting GATT server operation timed out.");
    return nullptr;
  }
  if (!blockServer || blockError != nil) {
    return nullptr;
  }
  return std::make_unique<GattServer>(blockServer);
}

std::unique_ptr<api::ble::GattClient> BleMedium::ConnectToGattServer(
    api::ble::BlePeripheral::UniqueId peripheral_id, api::ble::TxPowerLevel tx_power_level,
    api::ble::ClientGattConnectionCallback callback) {
  id<GNCPeripheral> peripheral = peripherals_.Get(peripheral_id);
  if (!peripheral) {
    GNCLoggerError(@"[NEARBY] Failed to connect to Gatt server: peripheral is not found.");
    return nullptr;
  }

  api::ble::ClientGattConnectionCallback thread_callback = {
      .on_characteristic_changed_cb =
          [this,
           call_on_characteristic_changed_cb = std::move(callback.on_characteristic_changed_cb)](
              absl::string_view characteristic_uuid) mutable {
            callback_executor_.Execute([characteristic_uuid = std::string(characteristic_uuid),
                                        call_on_characteristic_changed_cb = std::move(
                                            call_on_characteristic_changed_cb)]() mutable {
              call_on_characteristic_changed_cb(characteristic_uuid);
            });
          },
      .disconnected_cb =
          [this, call_disconnected_cb = std::move(callback.disconnected_cb)]() mutable {
            callback_executor_.Execute(
                [call_disconnected_cb = std::move(call_disconnected_cb)]() mutable {
                  call_disconnected_cb();
                });
          }};

  __block api::ble::ClientGattConnectionCallback blockCallback = std::move(thread_callback);

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block GNCBLEGATTClient *blockClient = nil;
  __block NSError *blockError = nil;
  [medium_ connectToGATTServerForPeripheral:peripheral
      disconnectionHandler:^(void) {
        blockCallback.disconnected_cb();
      }
      completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
        if (error != nil) {
          GNCLoggerError(@"Error connecting to GATT server: %@", error);
          blockError = error;
        } else {
          GNCLoggerInfo(@"Connecting to GATT server operation completed.");
          blockClient = client;
        }
        dispatch_semaphore_signal(semaphore);
      }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"Connecting to GATT server operation timed out.");
    return nullptr;
  }
  if (!blockClient || blockError != nil) {
    return nullptr;
  }
  return std::make_unique<GattClient>(blockClient);
}

// TODO(b/293336684): Old Weave code that need to be deleted once shared Weave is complete.
std::unique_ptr<api::ble::BleServerSocket> BleMedium::OpenServerSocket(
    const std::string &service_id) {
  auto server_socket = std::make_unique<BleServerSocket>();
  __block auto server_socket_ptr = server_socket.get();

  if (socketPeripheralManager_) {
    [socketPeripheralManager_ stop];
  }

  socketPeripheralManager_ = [[GNSPeripheralManager alloc] initWithAdvertisedName:nil
                                                                restoreIdentifier:nil];

  if (socketPeripheralManager_ == nil) {
    GNCLoggerError(@"Failed to create peripheral manager.");
    return nullptr;
  }

  socketPeripheralServiceManager_ = [[GNSPeripheralServiceManager alloc]
         initWithBleServiceUUID:[CBUUID UUIDWithString:kWeaveServiceUUID]
       addPairingCharacteristic:NO
      shouldAcceptSocketHandler:^BOOL(GNSSocket *socket) {
        GNCMWaitForConnection(socket, ^(BOOL didConnect) {
          GNCMBleConnection *connection =
              [GNCMBleConnection connectionWithSocket:socket
                                            // This must be nil as the advertiser even though we
                                            // have a service ID available to us.
                                            serviceID:nil
                                  expectedIntroPacket:YES
                                        callbackQueue:connection_callback_queue_];

          auto socket = std::make_unique<BleSocket>(connection);
          socket->SetCloseNotifier([socketPeripheralManager = socketPeripheralManager_,
                                    serviceUUID = socketPeripheralServiceManager_.serviceUUID]() {
            [socketPeripheralManager
                removePeripheralServiceManagerForServiceUUID:serviceUUID
                                 bleServiceRemovedCompletion:^(NSError *_Nullable error) {
                                   GNCLoggerInfo(@"BleSocket is removed peripheral manager.");
                                 }];
          });

          connection.connectionHandlers = socket->GetInputStream().GetConnectionHandlers();
          if (server_socket_ptr) {
            server_socket_ptr->Connect(std::move(socket));
          }

          GNCLoggerInfo(@"BleServerSocket is created with connection");
        });
        return YES;
      }];

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [socketPeripheralManager_ addPeripheralServiceManager:socketPeripheralServiceManager_
                              bleServiceAddedCompletion:^(NSError *error) {
                                if (error != nil) {
                                  GNCLoggerError(@"Failed to add Weave service: %@", error);
                                  blockError = error;
                                }
                                dispatch_semaphore_signal(semaphore);
                              }];
  [socketPeripheralManager_ start];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"OpenServerSocket operation timed out.");
    return nullptr;
  }
  if (blockError != nil) {
    return nullptr;
  }
  return std::move(server_socket);
}

std::unique_ptr<api::ble::BleL2capServerSocket> BleMedium::OpenL2capServerSocket(
    const std::string &service_id) {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockPSMPublishedError = nil;
  auto l2cap_server_socket = std::make_unique<BleL2capServerSocket>();
  l2cap_server_socket->SetCloseNotifier([this]() {
    absl::MutexLock lock(&l2cap_server_socket_mutex_);
    l2cap_server_socket_ptr_ = nullptr;
  });
  {
    absl::MutexLock lock(&l2cap_server_socket_mutex_);
    l2cap_server_socket_ptr_ = l2cap_server_socket.get();
  }
  std::string service_id_str = service_id;
  [medium_
      openL2CAPServerWithPSMPublishedCompletionHandler:^(uint16_t PSM, NSError *_Nullable error) {
        if (error) {
          blockPSMPublishedError = error;
          dispatch_semaphore_signal(semaphore);
          return;
        }
        {
          absl::MutexLock lock(&l2cap_server_socket_mutex_);
          if (l2cap_server_socket_ptr_) {
            l2cap_server_socket_ptr_->SetPSM(PSM);
          }
        }
        dispatch_semaphore_signal(semaphore);
      }
      channelOpenedCompletionHandler:^(GNCBLEL2CAPStream *_Nullable stream,
                                       NSError *_Nullable error) {
        if (error != nil) {
          GNCLoggerError(@"Error opening L2CAP channel in L2CAP server: %@", error);
          return;
        }
        GNCBLEL2CAPConnection *connection =
            [GNCBLEL2CAPConnection connectionWithStream:stream
                                              serviceID:@(service_id_str.c_str())
                                     incomingConnection:YES
                                          callbackQueue:connection_callback_queue_];
        auto socket = std::make_unique<BleL2capSocket>(connection);
        {
          absl::MutexLock lock(&l2cap_server_socket_mutex_);
          if (l2cap_server_socket_ptr_) {
            l2cap_server_socket_ptr_->AddPendingSocket(std::move(socket));
          }
        }
      }
      peripheralManager:nil];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"OpenL2capServerSocket operation timed out.");
    return nullptr;
  }
  if (blockPSMPublishedError != nil) {
    return nullptr;
  }

  return std::move(l2cap_server_socket);
}

// TODO(b/290385712): Add support for @c cancellation_flag.
// TODO(b/293336684): Old Weave code that need to be deleted once shared Weave is complete.
std::unique_ptr<api::ble::BleSocket> BleMedium::Connect(
    const std::string &service_id, api::ble::TxPowerLevel tx_power_level,
    api::ble::BlePeripheral::UniqueId peripheral_id, CancellationFlag *cancellation_flag) {
  id<GNCPeripheral> peripheral = peripherals_.Get(peripheral_id);
  if (!peripheral) {
    GNCLoggerError(@"Failed to connect to Gatt server: peripheral is not found.");
    return nullptr;
  }

  GNSCentralPeerManager *updatedCentralPeerManager =
      [socketCentralManager_ retrieveCentralPeerWithIdentifier:peripheral.identifier];
  if (!updatedCentralPeerManager) {
    return nullptr;
  }

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block std::unique_ptr<BleSocket> socket;
  [updatedCentralPeerManager
      socketWithPairingCharacteristic:NO
                           completion:^(GNSSocket *nssocket, NSError *error) {
                             if (error) {
                               dispatch_semaphore_signal(semaphore);
                               return;
                             }
                             GNCMWaitForConnection(nssocket, ^(BOOL didConnect) {
                               if (!didConnect) {
                                 dispatch_semaphore_signal(semaphore);
                                 return;
                               }

                               GNCMBleConnection *connection = [GNCMBleConnection
                                   connectionWithSocket:nssocket
                                              serviceID:@(service_id.c_str())
                                    expectedIntroPacket:NO
                                          callbackQueue:connection_callback_queue_];
                               socket = std::make_unique<BleSocket>(connection, peripheral_id);
                               connection.connectionHandlers =
                                   socket->GetInputStream().GetConnectionHandlers();
                               dispatch_semaphore_signal(semaphore);
                             });
                           }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"Failed to connect BLE socket: timeout.");
    return nullptr;
  }
  if (socket == nullptr) {
    return nullptr;
  }

  // Send the (empty) intro packet, which the BLE advertiser is expecting.
  socket->GetOutputStream().Write(ByteArray());
  return std::move(socket);
}

std::unique_ptr<api::ble::BleL2capSocket> BleMedium::ConnectOverL2cap(
    int psm, const std::string &service_id, api::ble::TxPowerLevel tx_power_level,
    api::ble::BlePeripheral::UniqueId peripheral_id, CancellationFlag *cancellation_flag) {
  id<GNCPeripheral> peripheral = peripherals_.Get(peripheral_id);
  if (!peripheral) {
    GNCLoggerError(@"Failed to connect over L2CAP: peripheral is not found.");
    return nullptr;
  }
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block std::unique_ptr<BleL2capSocket> socket;
  __block NSError *openError = nil;
  const std::string &service_id_str = service_id;
  [medium_ openL2CAPChannelWithPSM:psm
                        peripheral:peripheral
                 completionHandler:^(GNCBLEL2CAPStream *stream, NSError *error) {
                   if (error) {
                     openError = error;
                     dispatch_semaphore_signal(semaphore);
                     return;
                   }
                   GNCBLEL2CAPConnection *connection =
                       [GNCBLEL2CAPConnection connectionWithStream:stream
                                                         serviceID:@(service_id_str.c_str())
                                                incomingConnection:NO
                                                     callbackQueue:connection_callback_queue_];
                   // Blocked call to wait for the packet validation result.
                   // TODO: b/419654808 - Remove this once the packet validation is moved to the
                   // Connections layer.
                   [connection requestDataConnectionWithCompletion:^(BOOL result) {
                     if (result) {
                       socket = std::make_unique<BleL2capSocket>(connection, peripheral_id);
                     }
                     GNCLoggerInfo(result ? @"[NEARBY] Request data connection is ok"
                                          : @"[NEARBY] Request data connection is not ok");
                     dispatch_semaphore_signal(semaphore);
                   }];
                 }];
  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, kApiTimeoutInSeconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"Failed to connect over L2CAP: timeout.");
    return nullptr;
  }
  if (socket == nullptr) {
    if (openError != nil) {
      GNCLoggerError(@"Failed to connect over L2CAP:%@.", openError);
    }
    return nullptr;
  }

  return std::move(socket);
}

bool BleMedium::IsExtendedAdvertisementsAvailable() {
  return [medium_ supportsExtendedAdvertisements];
}

std::optional<api::ble::BlePeripheral::UniqueId> BleMedium::RetrieveBlePeripheralIdFromNativeId(
    const std::string &ble_peripheral_native_id) {
  NSString *uuidString = [[NSString alloc] initWithUTF8String:ble_peripheral_native_id.c_str()];
  if (uuidString == nil) {
    GNCLoggerError(@"Empty native BLE peripheral ID is invalid.");
    return std::nullopt;
  }

  NSUUID *uuidFromString = [[NSUUID alloc] initWithUUIDString:uuidString];
  if (uuidFromString == nil) {
    GNCLoggerError(@"Native BLE peripheral ID is not a valid UUID.");
    return std::nullopt;
  }

  id<GNCPeripheral> peripheral = peripherals_.Get(uuidFromString.hash);
  if (peripheral) {
    // Already has the BLE peripheral, return the Nearby Connections BLE peripheral id.
    return peripheral.identifier.hash;
  }

  // Retrieve the BLE peripheral from CBCentralManager.
  peripheral = [socketCentralManager_ retrievePeripheralWithIdentifier:uuidFromString];
  return peripheral.identifier.hash;
}

void BleMedium::ClearAdvertisementPacketsMap() {
  absl::MutexLock lock(&advertisement_packets_mutex_);
  advertisement_packets_map_.clear();
  last_timestamp_to_clean_expired_advertisement_packets_ = [NSDate date];
}

NSDate *BleMedium::GetLastTimestampToCleanExpiredAdvertisementPackets() {
  absl::MutexLock lock(&advertisement_packets_mutex_);
  return last_timestamp_to_clean_expired_advertisement_packets_;
}

void BleMedium::CleanUpExpiredAdvertisementPackets(NSDate *now) {
  absl::MutexLock lock(&advertisement_packets_mutex_);
  for (auto it = advertisement_packets_map_.begin(); it != advertisement_packets_map_.end();) {
    if ([now timeIntervalSinceDate:it->second.last_timestamp] >
        kAdvertisementPacketsMapExpirationTimeInterval) {
      advertisement_packets_map_.erase(it++);
    } else {
      ++it;
    }
  }
  last_timestamp_to_clean_expired_advertisement_packets_ = now;
}

bool BleMedium::ShouldReportAdvertisement(NSDate *now,
                                          api::ble::BlePeripheral::UniqueId peripheral_id,
                                          NSDictionary<CBUUID *, NSData *> *service_data) {
  absl::MutexLock lock(&advertisement_packets_mutex_);
  if (service_data == nil || service_data.count == 0) {
    return false;
  }

  auto it = advertisement_packets_map_.find(peripheral_id);
  if (it == advertisement_packets_map_.end()) {
    advertisement_packets_map_[peripheral_id] = {now, service_data};
    return true;
  }

  if ([now timeIntervalSinceDate:it->second.last_timestamp] < kThresholdInterval &&
          it -> second.last_service_data.count == service_data.count) {
    bool is_same_advertisement = true;
    for (CBUUID *service_uuid in service_data.allKeys) {
      if (![service_data[service_uuid] isEqualToData:it->second.last_service_data[service_uuid]]) {
        is_same_advertisement = false;
        break;
      }
    }
    if (is_same_advertisement) {
      return false;
    }
  }

  it->second.last_timestamp = now;
  it->second.last_service_data = service_data;
  return true;
}

void BleMedium::AddAdvertisementPacketInfo(api::ble::BlePeripheral::UniqueId peripheral_id,
                                           NSDictionary<CBUUID *, NSData *> *service_data) {
  absl::MutexLock lock(&advertisement_packets_mutex_);
  advertisement_packets_map_[peripheral_id] = {[NSDate date], service_data};
}

api::ble::BlePeripheral::UniqueId BleMedium::PeripheralsMap::Add(id<GNCPeripheral> peripheral) {
  absl::MutexLock lock(&mutex_);
  api::ble::BlePeripheral::UniqueId peripheral_id = peripheral.identifier.hash;
  peripherals_.insert({peripheral_id, peripheral});
  return peripheral_id;
}

id<GNCPeripheral> BleMedium::PeripheralsMap::Get(api::ble::BlePeripheral::UniqueId peripheral_id) {
  absl::MutexLock lock(&mutex_);
  auto peripheral_it = peripherals_.find(peripheral_id);
  if (peripheral_it == peripherals_.end()) {
    return nil;
  }
  return peripheral_it->second;
}

void BleMedium::PeripheralsMap::Clear() {
  absl::MutexLock lock(&mutex_);
  peripherals_.clear();
}

}  // namespace apple
}  // namespace nearby
