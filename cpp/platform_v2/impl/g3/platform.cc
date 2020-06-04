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

#include "platform_v2/api/platform.h"

#include <atomic>
#include <memory>

#include "platform_v2/api/atomic_boolean.h"
#include "platform_v2/api/atomic_reference.h"
#include "platform_v2/api/ble.h"
#include "platform_v2/api/ble_v2.h"
#include "platform_v2/api/bluetooth_adapter.h"
#include "platform_v2/api/bluetooth_classic.h"
#include "platform_v2/api/condition_variable.h"
#include "platform_v2/api/count_down_latch.h"
#include "platform_v2/api/mutex.h"
#include "platform_v2/api/scheduled_executor.h"
#include "platform_v2/api/server_sync.h"
#include "platform_v2/api/settable_future.h"
#include "platform_v2/api/submittable_executor.h"
#include "platform_v2/api/webrtc.h"
#include "platform_v2/api/wifi.h"
#include "platform_v2/impl/g3/atomic_boolean.h"
#include "platform_v2/impl/g3/atomic_reference_any.h"
#include "platform_v2/impl/g3/bluetooth_adapter.h"
#include "platform_v2/impl/g3/condition_variable.h"
#include "platform_v2/impl/g3/count_down_latch.h"
#include "platform_v2/impl/g3/multi_thread_executor.h"
#include "platform_v2/impl/g3/mutex.h"
#include "platform_v2/impl/g3/scheduled_executor.h"
#include "platform_v2/impl/g3/settable_future_any.h"
#include "platform_v2/impl/g3/single_thread_executor.h"
#include "absl/base/integral_types.h"
#include "absl/memory/memory.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace api {

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateSingleThreadExecutor() {
  return absl::make_unique<g3::SingleThreadExecutor>();
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateMultiThreadExecutor(int max_concurrency) {
  return absl::make_unique<g3::MultiThreadExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  return absl::make_unique<g3::ScheduledExecutor>();
}

std::unique_ptr<AtomicReference<absl::any>>
ImplementationPlatform::CreateAtomicReferenceAny(absl::any initial_value) {
  return absl::make_unique<g3::AtomicReferenceAny>(initial_value);
}

std::unique_ptr<SettableFuture<absl::any>>
ImplementationPlatform::CreateSettableFutureAny() {
  return absl::make_unique<g3::SettableFutureAny>();
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  return absl::make_unique<g3::BluetoothAdapter>();
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return absl::make_unique<g3::CountDownLatch>(count);
}

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return absl::make_unique<g3::AtomicBoolean>(initial_value);
}

std::unique_ptr<BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium() {
  return std::unique_ptr<BluetoothClassicMedium>();
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium() {
  return std::unique_ptr<BleMedium>();
}

std::unique_ptr<ble_v2::BleMedium> ImplementationPlatform::CreateBleV2Medium() {
  return std::unique_ptr<ble_v2::BleMedium>();
}

std::unique_ptr<ServerSyncMedium>
ImplementationPlatform::CreateServerSyncMedium() {
  return std::unique_ptr<ServerSyncMedium>(/*new ServerSyncMediumImpl()*/);
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return std::unique_ptr<WifiMedium>();
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return std::unique_ptr<WifiLanMedium>();
}

std::unique_ptr<WebRtcSignalingMessenger>
ImplementationPlatform::CreateWebRtcSignalingMessenger(
    absl::string_view self_id) {
  return std::unique_ptr<WebRtcSignalingMessenger>(
      /*new FCMSignalingMessenger()*/);
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  if (mode == Mutex::Mode::kRecursive)
    return absl::make_unique<g3::RecursiveMutex>();
  else
    return absl::make_unique<g3::Mutex>(mode == Mutex::Mode::kRegular);
}

std::unique_ptr<ConditionVariable>
ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return std::unique_ptr<ConditionVariable>(
      new g3::ConditionVariable(static_cast<g3::Mutex*>(mutex)));
}

std::string ImplementationPlatform::GetDeviceId() {
  // TODO(alexchau): Get deviceId from base
  return "google3";
}

std::string ImplementationPlatform::GetPayloadPath(int64_t payload_id) {
  return "/tmp/" + std::to_string(payload_id);
}

}  // namespace api
}  // namespace nearby
}  // namespace location
