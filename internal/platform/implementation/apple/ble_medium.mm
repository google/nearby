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
#import "internal/platform/implementation/apple/utils.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "internal/platform/implementation/apple/ble_utils.h"
#include "internal/platform/implementation/apple/utils.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTClient.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEGATTServer.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEMedium.h"
#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCPeripheral.h"
#import "internal/platform/implementation/apple/ble_gatt_client.h"
#import "internal/platform/implementation/apple/ble_gatt_server.h"
#import "internal/platform/implementation/apple/ble_peripheral.h"
#import "internal/platform/implementation/apple/ble_server_socket.h"
#import "internal/platform/implementation/apple/ble_socket.h"
#import "internal/platform/implementation/apple/bluetooth_adapter_v2.h"
#import "GoogleToolboxForMac/GTMLogger.h"

// TODO(b/293336684): Old Weave imports that need to be deleted once shared Weave is complete.
#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleConnection.h"
#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleUtils.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Central/GNSCentralPeerManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager.h"

static NSString *const kWeaveServiceUUID = @"FEF3";

namespace nearby {
namespace apple {

BleMedium::BleMedium() : medium_([[GNCBLEMedium alloc] init]) {}

std::unique_ptr<api::ble_v2::BleMedium::AdvertisingSession> BleMedium::StartAdvertising(
    const api::ble_v2::BleAdvertisementData &advertising_data,
    api::ble_v2::AdvertiseParameters advertise_set_parameters,
    api::ble_v2::BleMedium::AdvertisingCallback callback) {
  NSMutableDictionary<CBUUID *, NSData *> *serviceData =
      ObjCServiceDataFromCPP(advertising_data.service_data);

  __block api::ble_v2::BleMedium::AdvertisingCallback blockCallback = std::move(callback);

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

bool BleMedium::StartAdvertising(const api::ble_v2::BleAdvertisementData &advertising_data,
                                 api::ble_v2::AdvertiseParameters advertise_set_parameters) {
  NSMutableDictionary<CBUUID *, NSData *> *serviceData =
      ObjCServiceDataFromCPP(advertising_data.service_data);

  [socketPeripheralManager_ start];

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ startAdvertisingData:serviceData
              completionHandler:^(NSError *error) {
                if (error != nil) {
                  GTMLoggerError(@"Failed to start advertising: %@", error);
                }
                blockError = error;
                dispatch_semaphore_signal(semaphore);
              }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  return blockError == nil;
}

bool BleMedium::StopAdvertising() {
  [socketPeripheralManager_ stop];

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ stopAdvertisingWithCompletionHandler:^(NSError *error) {
    if (error != nil) {
      GTMLoggerError(@"Failed to stop advertising: %@", error);
    }
    blockError = error;
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  return blockError == nil;
}

void BleMedium::HandleAdvertisementFound(id<GNCPeripheral> peripheral,
                                         NSDictionary<CBUUID *, NSData *> *serviceData) {
  absl::MutexLock lock(&peripherals_mutex_);
  [socketCentralManager_ retrievePeripheralWithIdentifier:peripheral.identifier
                                        advertisementData:@{}];

  api::ble_v2::BleAdvertisementData data;
  for (CBUUID *key in serviceData.allKeys) {
    data.service_data[CPPUUIDFromObjC(key)] = ByteArrayFromNSData(serviceData[key]);
  }

  // Add the peripheral to the map if we haven't discovered it yet.
  auto ble_peripheral = std::make_unique<BlePeripheral>(peripheral);
  auto unique_id = ble_peripheral->GetUniqueId();
  auto it = peripherals_.find(unique_id);
  if (it == peripherals_.end()) {
    peripherals_[unique_id] = std::move(ble_peripheral);
  }
  if (scanning_cb_.advertisement_found_cb) {
    scanning_cb_.advertisement_found_cb(*peripherals_[unique_id], data);
  }
  if (scan_cb_.advertisement_found_cb) {
    scan_cb_.advertisement_found_cb(*peripherals_[unique_id], data);
  }
}

std::unique_ptr<api::ble_v2::BleMedium::ScanningSession> BleMedium::StartScanning(
    const Uuid &service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
    api::ble_v2::BleMedium::ScanningCallback callback) {
  absl::MutexLock lock(&peripherals_mutex_);
  CBUUID *serviceUUID = CBUUID128FromCPP(service_uuid);
  scanning_cb_ = std::move(callback);

  // Clear the map of discovered peripherals only when we are starting a new scan. If we cleared the
  // map every time we stopped a scan, we would not be able to connect to peripherals that we
  // discovered in that scan session.
  peripherals_.clear();

  socketCentralManager_ = [[GNSCentralManager alloc] initWithSocketServiceUUID:serviceUUID];
  [socketCentralManager_ startNoScanModeWithAdvertisedServiceUUIDs:@[ serviceUUID ]];

  [medium_ startScanningForService:serviceUUID
      advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                  NSDictionary<CBUUID *, NSData *> *serviceData) {
        HandleAdvertisementFound(peripheral, serviceData);
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

bool BleMedium::StartScanning(const Uuid &service_uuid, api::ble_v2::TxPowerLevel tx_power_level,
                              api::ble_v2::BleMedium::ScanCallback callback) {
  absl::MutexLock lock(&peripherals_mutex_);
  CBUUID *serviceUUID = CBUUID128FromCPP(service_uuid);
  scan_cb_ = std::move(callback);

  // Clear the map of discovered peripherals only when we are starting a new scan. If we cleared the
  // map every time we stopped a scan, we would not be able to connect to peripherals that we
  // discovered in that scan session.
  peripherals_.clear();

  socketCentralManager_ = [[GNSCentralManager alloc] initWithSocketServiceUUID:serviceUUID];
  [socketCentralManager_ startNoScanModeWithAdvertisedServiceUUIDs:@[ serviceUUID ]];

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ startScanningForService:serviceUUID
      advertisementFoundHandler:^(id<GNCPeripheral> peripheral,
                                  NSDictionary<CBUUID *, NSData *> *serviceData) {
        HandleAdvertisementFound(peripheral, serviceData);
      }
      completionHandler:^(NSError *error) {
        if (error != nil) {
          GTMLoggerError(@"Failed to start scanning: %@", error);
        }
        blockError = error;
        dispatch_semaphore_signal(semaphore);
      }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  return blockError == nil;
}

bool BleMedium::StopScanning() {
  [socketCentralManager_ stopNoScanMode];

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [medium_ stopScanningWithCompletionHandler:^(NSError *error) {
    if (error != nil) {
      GTMLoggerError(@"Failed to stop scanning: %@", error);
    }
    blockError = error;
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  return blockError == nil;
}

// TODO(b/290385712): Add implementation that calls ServerGattConnectionCallback methods.
std::unique_ptr<api::ble_v2::GattServer> BleMedium::StartGattServer(
    api::ble_v2::ServerGattConnectionCallback callback) {
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block GNCBLEGATTServer *blockServer = nil;
  [medium_ startGATTServerWithCompletionHandler:^(GNCBLEGATTServer *server, NSError *error) {
    if (error != nil) {
      GTMLoggerError(@"Error starting GATT server: %@", error);
    }
    blockServer = server;
    dispatch_semaphore_signal(semaphore);
  }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  if (!blockServer) {
    return nullptr;
  }
  return std::make_unique<GattServer>(blockServer);
}

std::unique_ptr<api::ble_v2::GattClient> BleMedium::ConnectToGattServer(
    api::ble_v2::BlePeripheral &peripheral, api::ble_v2::TxPowerLevel tx_power_level,
    api::ble_v2::ClientGattConnectionCallback callback) {
  // Check that the @c api::ble_v2::BlePeripheral is a @c nearby::apple::BlePeripheral and not a
  // @c nearby::apple::EmptyBlePeripheral instance, so we can retreive the CBPeripheral object.
  BlePeripheral *non_empty_peripheral = dynamic_cast<BlePeripheral *>(&peripheral);
  if (non_empty_peripheral == nullptr) {
    return nullptr;
  }

  __block api::ble_v2::ClientGattConnectionCallback blockCallback = std::move(callback);

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block GNCBLEGATTClient *blockClient = nil;
  [medium_ connectToGATTServerForPeripheral:non_empty_peripheral->GetPeripheral()
      disconnectionHandler:^(void) {
        blockCallback.disconnected_cb();
      }
      completionHandler:^(GNCBLEGATTClient *client, NSError *error) {
        if (error != nil) {
          GTMLoggerError(@"Error connecting to GATT server: %@", error);
        }
        blockClient = client;
        dispatch_semaphore_signal(semaphore);
      }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  if (!blockClient) {
    return nullptr;
  }
  return std::make_unique<GattClient>(blockClient);
}

// TODO(b/293336684): Old Weave code that need to be deleted once shared Weave is complete.
std::unique_ptr<api::ble_v2::BleServerSocket> BleMedium::OpenServerSocket(
    const std::string &service_id) {
  auto server_socket = std::make_unique<BleServerSocket>();
  __block auto server_socket_ptr = server_socket.get();
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
                                        callbackQueue:dispatch_get_main_queue()];

          auto socket = std::make_unique<BleSocket>(connection);
          connection.connectionHandlers = socket->GetInputStream().GetConnectionHandlers();
          if (server_socket_ptr) {
            server_socket_ptr->Connect(std::move(socket));
          }
        });
        return YES;
      }];
  socketPeripheralManager_ = [[GNSPeripheralManager alloc] initWithAdvertisedName:nil
                                                                restoreIdentifier:nil];

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError *blockError = nil;
  [socketPeripheralManager_ addPeripheralServiceManager:socketPeripheralServiceManager_
                              bleServiceAddedCompletion:^(NSError *error) {
                                if (error != nil) {
                                  GTMLoggerError(@"Failed to add Weave service: %@", error);
                                }
                                blockError = error;
                                dispatch_semaphore_signal(semaphore);
                              }];
  [socketPeripheralManager_ start];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  if (blockError != nil) {
    return nullptr;
  }
  return std::move(server_socket);
}

// TODO(b/290385712): Add support for @c cancellation_flag.
// TODO(b/293336684): Old Weave code that need to be deleted once shared Weave is complete.
std::unique_ptr<api::ble_v2::BleSocket> BleMedium::Connect(const std::string &service_id,
                                                           api::ble_v2::TxPowerLevel tx_power_level,
                                                           api::ble_v2::BlePeripheral &peripheral,
                                                           CancellationFlag *cancellation_flag) {
  // Check that the @c api::ble_v2::BlePeripheral is a @c nearby::apple::BlePeripheral and not a
  // @c nearby::apple::EmptyBlePeripheral instance, so we can retreive the CBPeripheral object.
  BlePeripheral *non_empty_peripheral = dynamic_cast<BlePeripheral *>(&peripheral);
  if (non_empty_peripheral == nullptr) {
    return nullptr;
  }

  GNSCentralPeerManager *updatedCentralPeerManager = [socketCentralManager_
      retrieveCentralPeerWithIdentifier:non_empty_peripheral->GetPeripheral().identifier];
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
                                          callbackQueue:dispatch_get_main_queue()];
                               socket =
                                   std::make_unique<BleSocket>(connection, non_empty_peripheral);
                               connection.connectionHandlers =
                                   socket->GetInputStream().GetConnectionHandlers();
                               dispatch_semaphore_signal(semaphore);
                             });
                           }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  if (socket == nullptr) {
    return nullptr;
  }

  // Send the (empty) intro packet, which the BLE advertiser is expecting.
  socket->GetOutputStream().Write(ByteArray());
  return std::move(socket);
}

bool BleMedium::IsExtendedAdvertisementsAvailable() {
  return [medium_ supportsExtendedAdvertisements];
}

bool BleMedium::GetRemotePeripheral(const std::string &mac_address,
                                    api::ble_v2::BleMedium::GetRemotePeripheralCallback callback) {
  // Apple does not expose MAC address information, so we cannot retreive a peripheral via MAC
  // address.
  return false;
}

bool BleMedium::GetRemotePeripheral(api::ble_v2::BlePeripheral::UniqueId unique_id,
                                    api::ble_v2::BleMedium::GetRemotePeripheralCallback callback) {
  // If the unique_id is 0, that means it's the local/empty peripheral. We must return "true"
  // otherwise the connection will be considered invalid and the application will crash.
  if (unique_id == 0) {
    callback(*local_peripheral_);
    return true;
  }

  BlePeripheral *peripheral;
  {
    absl::MutexLock lock(&peripherals_mutex_);
    auto it = peripherals_.find(unique_id);
    if (it == peripherals_.end()) {
      return false;
    }
    peripheral = it->second.get();
    if (peripheral == nullptr) {
      return false;
    }
  }
  // We need to unlock before calling the callback, otherwise we will deadlock.
  callback(*peripheral);
  return true;
}

}  // namespace apple
}  // namespace nearby
