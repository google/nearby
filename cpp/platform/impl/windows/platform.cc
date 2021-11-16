// Copyright 2021 Google LLC
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

#include <shlobj.h>

#include "platform/impl/shared/count_down_latch.h"
#include "platform/impl/shared/file.h"
#include "platform/impl/windows/atomic_boolean.h"
#include "platform/impl/windows/atomic_reference.h"
#include "platform/impl/windows/ble.h"
#include "platform/impl/windows/bluetooth_adapter.h"
#include "platform/impl/windows/bluetooth_classic_medium.h"
#include "platform/impl/windows/cancelable.h"
#include "platform/impl/windows/condition_variable.h"
#include "platform/impl/windows/executor.h"
#include "platform/impl/windows/future.h"
#include "platform/impl/windows/listenable_future.h"
#include "platform/impl/windows/log_message.h"
#include "platform/impl/windows/mutex.h"
#include "platform/impl/windows/scheduled_executor.h"
#include "platform/impl/windows/server_sync.h"
#include "platform/impl/windows/settable_future.h"
#include "platform/impl/windows/submittable_executor.h"
#include "platform/impl/windows/webrtc.h"
#include "platform/impl/windows/wifi.h"
#include "platform/impl/windows/wifi_lan.h"

namespace location {
namespace nearby {
namespace api {
namespace {

std::string GetPayloadPath(PayloadId payload_id) {
  PWSTR basePath;

  // Retrieves the full path of a known folder identified by the folder's
  // KNOWNFOLDERID.
  // https://docs.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
  SHGetKnownFolderPath(
      FOLDERID_Downloads,  //  rfid: A reference to the KNOWNFOLDERID that
                           //  identifies the folder.
      0,           // dwFlags: Flags that specify special retrieval options.
      NULL,        // hToken: An access token that represents a particular user.
      &basePath);  // ppszPath: When this method returns, contains the address
                   // of a pointer to a null-terminated Unicode string that
                   // specifies the path of the known folder. The calling
                   // process is responsible for freeing this resource once it
                   // is no longer needed by calling CoTaskMemFree, whether
                   // SHGetKnownFolderPath succeeds or not.

  char* fullpathUTF8 = new char((wcslen(basePath) + 1) * sizeof(char));
  wcstombs(fullpathUTF8, basePath, (wcslen(basePath) + 1) * sizeof(char));
  std::string fullPath = std::string(fullpathUTF8);
  auto retval = absl::StrCat(fullPath += "/", payload_id);
  return retval;
}
}  // namespace

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return absl::make_unique<windows::AtomicBoolean>();
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t value) {
  return absl::make_unique<windows::AtomicUint32>();
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return absl::make_unique<shared::CountDownLatch>(count);
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  return absl::make_unique<windows::Mutex>(mode);
}

std::unique_ptr<ConditionVariable>
ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return absl::make_unique<windows::ConditionVariable>(mutex);
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    const char* file_path) {
  return absl::make_unique<shared::InputFile>(file_path);
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    const char* file_path) {
  return absl::make_unique<shared::OutputFile>(file_path);
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* file, int line, LogMessage::Severity severity) {
  return absl::make_unique<windows::LogMessage>(file, line, severity);
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateSingleThreadExecutor() {
  return absl::make_unique<windows::SubmittableExecutor>();
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateMultiThreadExecutor(
    std::int32_t max_concurrency) {
  return absl::make_unique<windows::SubmittableExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  return absl::make_unique<windows::ScheduledExecutor>();
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  return absl::make_unique<windows::BluetoothAdapter>();
}

std::unique_ptr<BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    nearby::api::BluetoothAdapter& adapter) {
  return absl::make_unique<windows::BluetoothClassicMedium>(adapter);
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(
    BluetoothAdapter& adapter) {
  return absl::make_unique<windows::BleMedium>();
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<ble_v2::BleMedium> ImplementationPlatform::CreateBleV2Medium(
    BluetoothAdapter&) {
  return nullptr;
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<ServerSyncMedium>
ImplementationPlatform::CreateServerSyncMedium() {
  return std::unique_ptr<windows::ServerSyncMedium>();
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return std::unique_ptr<WifiMedium>();
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return absl::make_unique<windows::WifiLanMedium>();
}

// TODO(b/184975123): replace with real implementation.
std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() {
  return absl::make_unique<windows::WebRtcMedium>();
}

}  // namespace api
}  // namespace nearby
}  // namespace location
