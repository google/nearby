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
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/count_down_latch.h"
#include "internal/platform/implementation/credential_storage.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/implementation/http_loader.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/log_message.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/implementation/output_file.h"
#include "internal/platform/implementation/preferences_manager.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/implementation/server_sync.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/implementation/timer.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/os_name.h"
#include "internal/platform/payload_id.h"
#include "thread/thread.h"
#ifndef NO_WEBRTC
#include "internal/platform/implementation/g3/webrtc.h"
#include "internal/platform/implementation/webrtc.h"
#endif
#include "internal/platform/implementation/g3/atomic_boolean.h"
#include "internal/platform/implementation/g3/atomic_reference.h"
#include "internal/platform/implementation/g3/ble.h"
#include "internal/platform/implementation/g3/ble_v2.h"
#include "internal/platform/implementation/g3/bluetooth_adapter.h"
#include "internal/platform/implementation/g3/bluetooth_classic.h"
#include "internal/platform/implementation/g3/condition_variable.h"
#include "internal/platform/implementation/g3/credential_storage_impl.h"
#include "internal/platform/implementation/g3/device_info.h"
#include "internal/platform/implementation/g3/multi_thread_executor.h"
#include "internal/platform/implementation/g3/mutex.h"
#include "internal/platform/implementation/g3/preferences_manager.h"
#include "internal/platform/implementation/g3/scheduled_executor.h"
#include "internal/platform/implementation/g3/single_thread_executor.h"
#include "internal/platform/implementation/g3/timer.h"
#include "internal/platform/implementation/g3/wifi.h"
#include "internal/platform/implementation/g3/wifi_direct.h"
#include "internal/platform/implementation/g3/wifi_hotspot.h"
#include "internal/platform/implementation/g3/wifi_lan.h"
#include "internal/platform/implementation/shared/file.h"
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace api {

std::string ImplementationPlatform::GetCustomSavePath(
    const std::string& parent_folder, const std::string& file_name) {
  return absl::StrCat(parent_folder, file_name);
}

std::string ImplementationPlatform::GetDownloadPath(
    const std::string& parent_folder, const std::string& file_name) {
  return absl::StrCat("/tmp/", file_name);
}

OSName ImplementationPlatform::GetCurrentOS() { return OSName::kLinux; }

int GetCurrentTid() {
  const LiveThread* my = Thread_GetMyLiveThread();
  return LiveThread_Pthread_TID(my);
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateSingleThreadExecutor() {
  return std::make_unique<g3::SingleThreadExecutor>();
}

std::unique_ptr<SubmittableExecutor>
ImplementationPlatform::CreateMultiThreadExecutor(int max_concurrency) {
  return std::make_unique<g3::MultiThreadExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  return std::make_unique<g3::ScheduledExecutor>();
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t value) {
  return std::make_unique<g3::AtomicUint32>(value);
}

std::unique_ptr<BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  return std::make_unique<g3::BluetoothAdapter>();
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(
    std::int32_t count) {
  return std::make_unique<shared::CountDownLatch>(count);
}

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return std::make_unique<g3::AtomicBoolean>(initial_value);
}

ABSL_DEPRECATED("This interface will be deleted in the near future.")
std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    PayloadId payload_id, std::int64_t total_size) {
  std::string parent_folder("");
  std::string file_name(std::to_string(payload_id));
  return shared::IOFile::CreateInputFile(
      GetDownloadPath(parent_folder, file_name), total_size);
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    const std::string& file_path, size_t size) {
  return shared::IOFile::CreateInputFile(file_path, size);
}

ABSL_DEPRECATED("This interface will be deleted in the near future.")
std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    PayloadId payload_id) {
  std::string parent_folder("");
  std::string file_name(std::to_string(payload_id));

  return shared::IOFile::CreateOutputFile(
      GetDownloadPath(parent_folder, file_name));
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    const std::string& file_path) {
  return shared::IOFile::CreateOutputFile(file_path);
}

std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* file, int line, LogMessage::Severity severity) {
  return nullptr;
}

std::unique_ptr<BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    api::BluetoothAdapter& adapter) {
  return std::make_unique<g3::BluetoothClassicMedium>(adapter);
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(
    api::BluetoothAdapter& adapter) {
  return std::make_unique<g3::BleMedium>(adapter);
}

std::unique_ptr<api::ble_v2::BleMedium>
ImplementationPlatform::CreateBleV2Medium(api::BluetoothAdapter& adapter) {
  return std::make_unique<g3::BleV2Medium>(adapter);
}

std::unique_ptr<api::CredentialStorage>
ImplementationPlatform::CreateCredentialStorage() {
  return std::make_unique<g3::CredentialStorageImpl>();
}

std::unique_ptr<ServerSyncMedium>
ImplementationPlatform::CreateServerSyncMedium() {
  return std::unique_ptr<ServerSyncMedium>(/*new ServerSyncMediumImpl()*/);
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  return std::make_unique<g3::WifiMedium>();
}

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return std::make_unique<g3::WifiLanMedium>();
}

std::unique_ptr<WifiHotspotMedium>
ImplementationPlatform::CreateWifiHotspotMedium() {
  return std::make_unique<g3::WifiHotspotMedium>();
}

std::unique_ptr<WifiDirectMedium>
ImplementationPlatform::CreateWifiDirectMedium() {
  return std::make_unique<g3::WifiDirectMedium>();
}

#ifndef NO_WEBRTC
std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() {
  if (MediumEnvironment::Instance().GetEnvironmentConfig().webrtc_enabled) {
    return std::make_unique<g3::WebRtcMedium>();
  } else {
    return nullptr;
  }
}
#endif

absl::StatusOr<WebResponse> ImplementationPlatform::SendRequest(
    const api::WebRequest& request) {
  return absl::UnimplementedError("");
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  if (mode == Mutex::Mode::kRecursive)
    return std::make_unique<g3::RecursiveMutex>();
  else
    return std::make_unique<g3::Mutex>(mode == Mutex::Mode::kRegular);
}

std::unique_ptr<ConditionVariable>
ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return std::unique_ptr<ConditionVariable>(
      new g3::ConditionVariable(static_cast<g3::Mutex*>(mutex)));
}

std::unique_ptr<Timer> ImplementationPlatform::CreateTimer() {
  return std::make_unique<g3::Timer>();
}

std::unique_ptr<nearby::api::DeviceInfo>
ImplementationPlatform::CreateDeviceInfo() {
  return std::make_unique<g3::DeviceInfo>();
}

std::unique_ptr<nearby::api::PreferencesManager>
ImplementationPlatform::CreatePreferencesManager(absl::string_view path) {
  return std::make_unique<g3::PreferencesManager>(path);
}

}  // namespace api
}  // namespace nearby
