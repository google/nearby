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

#include "fastpair/dart/windows/fast_pair_service_adapter_dart.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "third_party/dart_lang/v2/runtime/include/dart_api.h"
#include "fastpair/dart/proto/callbacks.proto.h"
#include "fastpair/dart/proto/enum.proto.h"
#include "fastpair/dart/windows/fast_pair_service_adapter.h"
#include "fastpair/keyed_service/fast_pair_mediator.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
namespace windows {
namespace {
class NotificationControllerObserver
    : public FastPairNotificationController::Observer {
 public:
  explicit NotificationControllerObserver(Dart_Port port)
      : callback_dart_(port) {}

  void OnUpdateDevice(const DeviceMetadata &device) override {
    NEARBY_LOGS(INFO) << __func__
                      << ": The on update device is triggered for dart. "
                      << device.GetDetails().name();
    const std::string device_proto =
        SerializeDeviceChanged(device).SerializeAsString();
    Dart_CObject object_device = {
        .type = Dart_CObject_Type::Dart_CObject_kTypedData,
        .value = {
            .as_typed_data{.type = Dart_TypedData_Type::Dart_TypedData_kUint8,
                           .length = (intptr_t)device_proto.size(),
                           .values = (uint8_t *)device_proto.data()}}};
    if (!Dart_PostCObject_DL(callback_dart_, &object_device)) {
      NEARBY_LOGS(ERROR)
          << "Failed to post OnContactsDownloaded message to dart. id = "
          << device.GetDetails().id();
    }
  }

 private:
  ::nearby::fastpair::dart::proto::DeviceDownloadedCallbackData
  SerializeDeviceChanged(const DeviceMetadata &device) {
    ::nearby::fastpair::dart::proto::DeviceDownloadedCallbackData callback;
    callback.add_devices()->MergeFrom(device.GetDetails());
    return callback;
  }
  Dart_Port callback_dart_;
};
}  // namespace

void InitMediatorDart() { InitMediator(); }

void StartScanDart(Mediator *pMediator) { StartScan(pMediator); }

static absl::flat_hash_map<Dart_Port,
                           std::unique_ptr<NotificationControllerObserver>>
    *notification_controller_observer_map_ = new absl::flat_hash_map<
        Dart_Port, std::unique_ptr<NotificationControllerObserver>>();

void AddNotificationControllerObserverDart(Mediator *pMediator,
                                           Dart_Port port) {
  auto observer = std::make_unique<NotificationControllerObserver>(port);
  AddNotificationControllerObserver(pMediator, observer.get());
  notification_controller_observer_map_->insert({port, std::move(observer)});
  CHECK(notification_controller_observer_map_->contains(port));
}

void RemoveNotificationControllerObserverDart(Mediator *pMediator,
                                              Dart_Port port) {
  auto it = notification_controller_observer_map_->find(port);
  if (it != notification_controller_observer_map_->end()) {
    RemoveNotificationControllerObserver(pMediator, it->second.get());
    notification_controller_observer_map_->erase(it);
  }
}

void DiscoveryClickedDart(Mediator *pMediator, int action) {
  switch (action) {
    case ::nearby::fastpair::dart::proto::DISCOVERY_ACTION_PAIR_TO_DEVICE:
      DiscoveryClicked(pMediator,
                       ::nearby::fastpair::DiscoveryAction::kPairToDevice);
      break;
    case ::nearby::fastpair::dart::proto::DISCOVERY_ACTION_LEARN_MORE:
      DiscoveryClicked(pMediator,
                       ::nearby::fastpair::DiscoveryAction::kLearnMore);
      break;
  }
}

}  // namespace windows
}  // namespace fastpair
}  // namespace nearby
