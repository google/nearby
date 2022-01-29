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

#include "internal/platform/implementation/platform.h"

#include <atomic>
#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/log_message.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/implementation/server_sync.h"
#include "internal/platform/implementation/submittable_executor.h"
#ifndef NO_WEBRTC
#include "internal/platform/implementation/g3/webrtc.h"
#include "internal/platform/implementation/webrtc.h"
#endif
#include "internal/platform/implementation/g3/atomic_boolean.h"
#include "internal/platform/implementation/g3/atomic_reference.h"
#include "internal/platform/implementation/g3/ble.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/bluetooth_classic.h"
#include "internal/platform/implementation/g3/condition_variable.h"
#include "internal/platform/implementation/g3/log_message.h"
#include "internal/platform/implementation/g3/multi_thread_executor.h"
#include "internal/platform/implementation/g3/mutex.h"
#include "internal/platform/implementation/g3/scheduled_executor.h"
#include "internal/platform/implementation/g3/single_thread_executor.h"
#include "internal/platform/implementation/g3/wifi_lan.h"
#include "internal/platform/implementation/shared/file.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/medium_environment.h"

namespace location {
namespace nearby {
namespace api {

namespace {
std::string GetPayloadPath(PayloadId payload_id) {
  return absl::StrCat("/tmp/", payload_id);
}
}  // namespace

int GetCurrentTid() {
  const LiveThread* my = Thread_GetMyLiveThread();
  return LiveThread_Pthread_TID(my);
}

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

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t value) {
  return absl::make_unique<g3::AtomicUint32>(value);
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  return absl::make_unique<g3::BluetoothAdapter>();
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return absl::make_unique<shared::CountDownLatch>(count);
}

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return absl::make_unique<g3::AtomicBoolean>(initial_value);
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    PayloadId payload_id, std::int64_t total_size) {
  return shared::IOFile::CreateInputFile(GetPayloadPath(payload_id),
                                         total_size);
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    PayloadId payload_id) {
  return shared::IOFile::CreateOutputFile(GetPayloadPath(payload_id));
}

std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* file, int line, LogMessage::Severity severity) {
  return absl::make_unique<g3::LogMessage>(file, line, severity);
}

std::unique_ptr<BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    api::BluetoothAdapter& adapter) {
  return absl::make_unique<g3::BluetoothClassicMedium>(adapter);
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(
    api::BluetoothAdapter& adapter) {
  return absl::make_unique<g3::BleMedium>(adapter);
}

std::unique_ptr<::nearby::cal::api::BleMedium>
ImplementationPlatform::CreateBleV2Medium(api::BluetoothAdapter& adapter) {
  // TODO(edwinwu): Implement g3 BLE v2.
  return nullptr;
}

std::unique_ptr<ServerSyncMedium>
ImplementationPlatform::CreateServerSyncMedium() {
  return std::unique_ptr<ServerSyncMedium>(/*new ServerSyncMediumImpl()*/);
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return std::unique_ptr<WifiMedium>();
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return absl::make_unique<g3::WifiLanMedium>();
}

#ifndef NO_WEBRTC
std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() {
  if (MediumEnvironment::Instance().GetEnvironmentConfig().webrtc_enabled) {
    return absl::make_unique<g3::WebRtcMedium>();
  } else {
    return nullptr;
  }
}
#endif

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

}  // namespace api
}  // namespace nearby
}  // namespace location
