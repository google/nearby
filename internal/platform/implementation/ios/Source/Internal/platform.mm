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

#include <string>

#import "internal/platform/implementation/ios/Source/Internal/GNCCore.h"
#include "internal/platform/implementation/ios/Source/Platform/atomic_boolean.h"
#include "internal/platform/implementation/ios/Source/Platform/atomic_uint32.h"
#include "internal/platform/implementation/ios/Source/Platform/condition_variable.h"
#include "internal/platform/implementation/ios/Source/Platform/count_down_latch.h"
#include "internal/platform/implementation/ios/Source/Platform/input_file.h"
#import "internal/platform/implementation/ios/Source/Platform/log_message.h"
#import "internal/platform/implementation/ios/Source/Platform/multi_thread_executor.h"
#include "internal/platform/implementation/ios/Source/Platform/mutex.h"
#import "internal/platform/implementation/ios/Source/Platform/scheduled_executor.h"
#import "internal/platform/implementation/ios/Source/Platform/single_thread_executor.h"
#import "internal/platform/implementation/ios/Source/Platform/utils.h"
#include "internal/platform/implementation/ios/Source/Platform/wifi_lan.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/implementation/shared/file.h"
#include "internal/platform/payload_id.h"

namespace location {
namespace nearby {
namespace api {

namespace {
std::string GetPayloadPath(PayloadId payload_id) {
  // This is to get a file path, e.g. /tmp/[payload_id], for the storage of payload file.
  // NOTE: Per
  // https://developer.apple.com/library/content/documentation/FileManagement/Conceptual/FileSystemProgrammingGuide/FileSystemOverview/FileSystemOverview.html
  // Files saved in the /tmp directory will be deleted by the system. Callers should be responsible
  // for copying the files to the permanent storage.
  NSString* payloadIdString = ObjCStringFromCppString(std::to_string(payload_id));
  return CppStringFromObjCString(
      [NSTemporaryDirectory() stringByAppendingPathComponent:payloadIdString]);
}
}  // namespace

// Atomics:
std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(bool initial_value) {
  return std::make_unique<ios::AtomicBoolean>(initial_value);
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t initial_value) {
  return std::make_unique<ios::AtomicUint32>(initial_value);
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(std::int32_t count) {
  return std::make_unique<ios::CountDownLatch>(count);
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  // iOS does not support unchecked Mutex in debug mode, therefore
  // ios::Mutex is used for both kRegular and kRegularNoCheck.
  if (mode == Mutex::Mode::kRecursive) {
    return absl::make_unique<ios::RecursiveMutex>();
  } else {
    return absl::make_unique<ios::Mutex>();
  }
}

std::unique_ptr<ConditionVariable> ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return std::make_unique<ios::ConditionVariable>(static_cast<ios::Mutex*>(mutex));
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(PayloadId payload_id,
                                                                   std::int64_t total_size) {
  // Extract the NSURL object with payload_id from |GNCCore| which stores the maps. If the retrieved
  // NSURL object is not nil, we create InputFile by ios::InputFile. The difference is
  // that ios::InputFile implements to read bytes from local real file for sending.
  GNCCore* core = GNCGetCore();
  NSURL* url = [core extractURLWithPayloadID:payload_id];
  if (url != nil) {
    return absl::make_unique<ios::InputFile>(url);
  } else {
    return shared::IOFile::CreateInputFile(GetPayloadPath(payload_id), total_size);
  }
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(PayloadId payload_id) {
  return shared::IOFile::CreateOutputFile(GetPayloadPath(payload_id));
}

std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* file, int line, LogMessage::Severity severity) {
  return absl::make_unique<ios::LogMessage>(file, line, severity);
}

// Java-like Executors
std::unique_ptr<SubmittableExecutor> ImplementationPlatform::CreateSingleThreadExecutor() {
  return std::make_unique<ios::SingleThreadExecutor>();
}

std::unique_ptr<SubmittableExecutor> ImplementationPlatform::CreateMultiThreadExecutor(
    int max_concurrency) {
  return std::make_unique<ios::MultiThreadExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor> ImplementationPlatform::CreateScheduledExecutor() {
  return std::make_unique<ios::ScheduledExecutor>();
}

// Mediums
std::unique_ptr<BluetoothAdapter> ImplementationPlatform::CreateBluetoothAdapter() {
  return nullptr;
}

std::unique_ptr<BluetoothClassicMedium> ImplementationPlatform::CreateBluetoothClassicMedium(
    api::BluetoothAdapter& adapter) {
  return nullptr;
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(api::BluetoothAdapter& adapter) {
  return nullptr;
}

std::unique_ptr<ble_v2::BleMedium>
ImplementationPlatform::CreateBleV2Medium(api::BluetoothAdapter& adapter) {
  return nullptr;
}

std::unique_ptr<ServerSyncMedium> ImplementationPlatform::CreateServerSyncMedium() {
  return nullptr;
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() { return nullptr; }

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return std::make_unique<ios::WifiLanMedium>();
}

std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() { return nullptr; }

}  // namespace api
}  // namespace nearby
}  // namespace location
