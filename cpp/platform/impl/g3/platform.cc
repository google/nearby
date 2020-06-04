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

#include "platform/api/platform.h"

#include <atomic>
#include <memory>

#include "platform/api/atomic_boolean.h"
#include "platform/api/atomic_reference.h"
#include "platform/api/ble.h"
#include "platform/api/ble_v2.h"
#include "platform/api/bluetooth_adapter.h"
#include "platform/api/bluetooth_classic.h"
#include "platform/api/condition_variable.h"
#include "platform/api/count_down_latch.h"
#include "platform/api/hash_utils.h"
#include "platform/api/lock.h"
#include "platform/api/scheduled_executor.h"
#include "platform/api/server_sync.h"
#include "platform/api/settable_future.h"
#include "platform/api/submittable_executor.h"
#include "platform/api/system_clock.h"
#include "platform/api/thread_utils.h"
#include "platform/api/webrtc.h"
#include "platform/api/wifi.h"
#include "platform/impl/g3/atomic_reference_impl.h"
#include "platform/impl/g3/settable_future_impl.h"
#include "platform/impl/g3/system_clock_impl.h"
#include "platform/impl/shared/atomic_boolean_impl.h"
#include "platform/impl/shared/file_impl.h"
#include "platform/impl/shared/posix_condition_variable.h"
#include "platform/impl/shared/posix_lock.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace platform {

namespace {
std::string getPayloadPath(std::int64_t payload_id) {
  return "/tmp/" + std::to_string(payload_id);
}
}  // namespace

Ptr<SubmittableExecutor> ImplementationPlatform::createSingleThreadExecutor() {
  return Ptr<SubmittableExecutor>(/*new SingleThreadExecutorImpl()*/);
}

Ptr<SubmittableExecutor> ImplementationPlatform::createMultiThreadExecutor(
    int max_concurrency) {
  return Ptr<SubmittableExecutor>(/*new MultiThreadExecutorImpl()*/);
}

Ptr<ScheduledExecutor> ImplementationPlatform::createScheduledExecutor() {
  return Ptr<ScheduledExecutor>(/*new ScheduledExecutorImpl()*/);
}

Ptr<AtomicReference<absl::any>>
ImplementationPlatform::createAtomicReferenceAny(absl::any initial_value) {
  return Ptr<AtomicReference<absl::any>>(
      new AtomicReferenceImpl(initial_value));
}

Ptr<SettableFuture<absl::any>>
ImplementationPlatform::createSettableFutureAny() {
  return Ptr<SettableFuture<std::any>>(new SettableFutureImpl{});
}

Ptr<BluetoothAdapter> ImplementationPlatform::createBluetoothAdapter() {
  return Ptr<BluetoothAdapter>{};
}

Ptr<WifiMedium> ImplementationPlatform::createWifiMedium() {
  return Ptr<WifiMedium>();
}

Ptr<CountDownLatch> ImplementationPlatform::createCountDownLatch(
    std::int32_t count) {
  return Ptr<CountDownLatch>(/*new CountDownLatchImpl(count)*/);
}

Ptr<ThreadUtils> ImplementationPlatform::createThreadUtils() {
  return Ptr<ThreadUtils>(/*new ThreadUtilsImpl()*/);
}

Ptr<SystemClock> ImplementationPlatform::createSystemClock() {
  return Ptr<SystemClock>(new SystemClockImpl());
}

Ptr<AtomicBoolean> ImplementationPlatform::createAtomicBoolean(
    bool initial_value) {
  return Ptr<AtomicBoolean>(new AtomicBooleanImpl(initial_value));
}

Ptr<InputFile> ImplementationPlatform::createInputFile(
    std::int64_t payload_id, std::int64_t total_size) {
  return MakePtr(new InputFileImpl(getPayloadPath(payload_id), total_size));
}

Ptr<OutputFile> ImplementationPlatform::createOutputFile(
    std::int64_t payload_id) {
  return MakePtr(new OutputFileImpl(getPayloadPath(payload_id)));
}

Ptr<BluetoothClassicMedium>
ImplementationPlatform::createBluetoothClassicMedium() {
  return Ptr<BluetoothClassicMedium>();
}

Ptr<BLEMedium> ImplementationPlatform::createBLEMedium() {
  return Ptr<BLEMedium>();
}

Ptr<BLEMediumV2> ImplementationPlatform::createBLEMediumV2() {
  return Ptr<BLEMediumV2>();
}

Ptr<ServerSyncMedium> ImplementationPlatform::createServerSyncMedium() {
  return Ptr<ServerSyncMedium>(/*new ServerSyncMediumImpl()*/);
}

Ptr<WifiLanMedium> ImplementationPlatform::createWifiLanMedium() {
  return Ptr<WifiLanMedium>();
}

//Ptr<WebRtcSignalingMessenger>
//ImplementationPlatform::createWebRtcSignalingMessenger(
//    const std::string& self_id) {
//  return Ptr<WebRtcSignalingMessenger>(/*new FCMSignalingMessenger()*/);
//}

Ptr<Lock> ImplementationPlatform::createLock() {
  return Ptr<Lock>(new PosixLock());
}

Ptr<ConditionVariable> ImplementationPlatform::createConditionVariable(
    Ptr<Lock> lock) {
  return Ptr<ConditionVariable>(new PosixConditionVariable(lock));
}

Ptr<HashUtils> ImplementationPlatform::createHashUtils() {
  return Ptr<HashUtils>(/*new HashUtilsImpl()*/);
}

std::string ImplementationPlatform::getDeviceId() {
  // TODO(alexchau): Get deviceId from base
  return "google3";
}

}  // namespace platform
}  // namespace nearby
}  // namespace location
