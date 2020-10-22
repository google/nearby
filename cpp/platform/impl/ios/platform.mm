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
#include "platform/api/condition_variable.h"
#include "platform/api/count_down_latch.h"
#include "platform/api/log_message.h"
#include "platform/api/mutex.h"
#include "platform/api/scheduled_executor.h"
#include "platform/api/submittable_executor.h"
#include "platform/impl/ios/atomic_boolean.h"
#include "platform/impl/ios/atomic_reference.h"
#include "platform/impl/ios/condition_variable.h"
#include "platform/impl/ios/count_down_latch.h"
#include "platform/impl/ios/log_message.h"
#include "platform/impl/ios/multi_thread_executor.h"
#include "platform/impl/ios/mutex.h"
#include "platform/impl/ios/scheduled_executor.h"
#include "platform/impl/ios/single_thread_executor.h"
#include "platform/impl/shared/file.h"
#include "absl/memory/memory.h"

namespace location {
namespace nearby {
namespace api {

namespace {
std::string GetPayloadPath(PayloadId payload_id) {
  return absl::StrCat("/tmp/", payload_id);
}
}  // namespace

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(bool initial_value) {
  return absl::make_unique<ios::AtomicBoolean>(initial_value);
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(std::uint32_t value) {
  return absl::make_unique<ios::AtomicUint32>(value);
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return absl::make_unique<ios::CountDownLatch>(count);
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  if (mode == Mutex::Mode::kRecursive)
    return absl::make_unique<ios::RecursiveMutex>();
  else
    return absl::make_unique<ios::Mutex>(mode == Mutex::Mode::kRegular);
}

std::unique_ptr<ConditionVariable> ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return std::unique_ptr<ConditionVariable>(
      new ios::ConditionVariable(static_cast<ios::Mutex*>(mutex)));
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(PayloadId payload_id,
                                                                   std::int64_t total_size) {
  return absl::make_unique<shared::InputFile>(GetPayloadPath(payload_id), total_size);
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(PayloadId payload_id) {
  return absl::make_unique<shared::OutputFile>(GetPayloadPath(payload_id));
}

std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* file, int line, LogMessage::Severity severity) {
  return absl::make_unique<ios::LogMessage>(file, line, severity);
}

std::unique_ptr<SubmittableExecutor> ImplementationPlatform::CreateSingleThreadExecutor() {
  return absl::make_unique<ios::SingleThreadExecutor>();
}

std::unique_ptr<SubmittableExecutor> ImplementationPlatform::CreateMultiThreadExecutor(
    int max_concurrency) {
  return absl::make_unique<ios::MultiThreadExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor> ImplementationPlatform::CreateScheduledExecutor() {
  return absl::make_unique<ios::ScheduledExecutor>();
}

std::unique_ptr<BluetoothAdapter> ImplementationPlatform::CreateBluetoothAdapter() {
  return std::unique_ptr<BluetoothAdapter>();
}

std::unique_ptr<BluetoothClassicMedium> ImplementationPlatform::CreateBluetoothClassicMedium(
    api::BluetoothAdapter& adapter) {
  return std::unique_ptr<BluetoothClassicMedium>();
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(api::BluetoothAdapter& adapter) {
  return std::unique_ptr<BleMedium>();
}

std::unique_ptr<ble_v2::BleMedium> ImplementationPlatform::CreateBleV2Medium(
    api::BluetoothAdapter& adapter) {
  return std::unique_ptr<ble_v2::BleMedium>();
}

std::unique_ptr<ServerSyncMedium> ImplementationPlatform::CreateServerSyncMedium() {
  return std::unique_ptr<ServerSyncMedium>();
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return std::unique_ptr<WifiMedium>();
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
   return std::unique_ptr<WifiLanMedium>();
}

std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() {
  return std::unique_ptr<WebRtcMedium>();
}

}  // namespace api
}  // namespace nearby
}  // namespace location
