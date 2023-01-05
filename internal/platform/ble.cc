// Copyright 2020 Google LLC
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

#include "internal/platform/ble.h"

#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {

bool BleMedium::StartAdvertising(
    const std::string& service_id, const ByteArray& advertisement_bytes,
    const std::string& fast_advertisement_service_uuid) {
  return impl_->StartAdvertising(service_id, advertisement_bytes,
                                 fast_advertisement_service_uuid);
}

bool BleMedium::StopAdvertising(const std::string& service_id) {
  return impl_->StopAdvertising(service_id);
}

bool BleMedium::StartScanning(
    const std::string& service_id,
    const std::string& fast_advertisement_service_uuid,
    DiscoveredPeripheralCallback callback) {
  {
    MutexLock lock(&mutex_);
    discovered_peripheral_callback_ = std::move(callback);
    peripherals_.clear();
  }
  return impl_->StartScanning(
      service_id, fast_advertisement_service_uuid,
      {
          .peripheral_discovered_cb =
              [this](api::BlePeripheral& peripheral,
                     const std::string& service_id, bool fast_advertisement) {
                MutexLock lock(&mutex_);
                auto pair = peripherals_.emplace(
                    &peripheral, absl::make_unique<ScanningInfo>());
                auto& context = *pair.first->second;
                if (pair.second) {
                  context.peripheral = BlePeripheral(&peripheral);
                  discovered_peripheral_callback_.peripheral_discovered_cb(
                      context.peripheral, service_id,
                      context.peripheral.GetAdvertisementBytes(service_id),
                      fast_advertisement);
                }
              },
          .peripheral_lost_cb =
              [this](api::BlePeripheral& peripheral,
                     const std::string& service_id) {
                MutexLock lock(&mutex_);
                if (peripherals_.empty()) return;
                auto context = peripherals_.find(&peripheral);
                if (context == peripherals_.end()) return;
                NEARBY_LOG(INFO, "Removing peripheral=%p, impl=%p",
                           &(context->second->peripheral), &peripheral);
                discovered_peripheral_callback_.peripheral_lost_cb(
                    context->second->peripheral, service_id);
              },
      });
}

bool BleMedium::StopScanning(const std::string& service_id) {
  {
    MutexLock lock(&mutex_);
    discovered_peripheral_callback_ = {};
    peripherals_.clear();
    NEARBY_LOG(INFO, "Ble Scanning disabled: impl=%p", &GetImpl());
  }
  return impl_->StopScanning(service_id);
}

bool BleMedium::StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback) {
  {
    MutexLock lock(&mutex_);
    accepted_connection_callback_ = std::move(callback);
  }
  return impl_->StartAcceptingConnections(
      service_id,
      {
          .accepted_cb =
              [this](api::BleSocket& socket, const std::string& service_id) {
                MutexLock lock(&mutex_);
                auto pair = sockets_.emplace(
                    &socket, absl::make_unique<AcceptedConnectionInfo>());
                auto& context = *pair.first->second;
                if (!pair.second) {
                  NEARBY_LOG(INFO, "Accepting (again) socket=%p, impl=%p",
                             &context.socket, &socket);
                } else {
                  context.socket = BleSocket(&socket);
                  NEARBY_LOG(INFO, "Accepting socket=%p, impl=%p",
                             &context.socket, &socket);
                }
                accepted_connection_callback_.accepted_cb(context.socket,
                                                          service_id);
              },
      });
}

bool BleMedium::StopAcceptingConnections(const std::string& service_id) {
  {
    MutexLock lock(&mutex_);
    accepted_connection_callback_ = {};
    sockets_.clear();
    NEARBY_LOG(INFO, "Ble accepted connection disabled: impl=%p", &GetImpl());
  }
  return impl_->StopAcceptingConnections(service_id);
}

BleSocket BleMedium::Connect(BlePeripheral& peripheral,
                             const std::string& service_id,
                             CancellationFlag* cancellation_flag) {
  {
    MutexLock lock(&mutex_);
    NEARBY_LOG(INFO, "BleMedium::Connect: peripheral=%p [impl=%p]", &peripheral,
               &peripheral.GetImpl());
  }
  return BleSocket(
      impl_->Connect(peripheral.GetImpl(), service_id, cancellation_flag));
}

}  // namespace nearby
