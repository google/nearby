// filepath: /workspace/internal/platform/implementation/linux/platform.cc
// Minimal Linux implementation of ImplementationPlatform.

#include "internal/platform/implementation/platform.h"

#include <memory>
#include <string>

#include "bluetooth_adapter.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/count_down_latch.h"
#include "internal/platform/implementation/http_loader.h"
#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/linux/atomics.h"
#include "internal/platform/implementation/linux/multi_thread_executor.h"
#include "internal/platform/implementation/linux/scheduled_executor.h"
#include "internal/platform/implementation/linux/device_info.h"
#include "internal/platform/implementation/shared/posix_mutex.h"
#include "internal/platform/implementation/shared/posix_condition_variable.h"


#include <sdbus-c++/sdbus-c++.h>
namespace nearby {
namespace api {

std::string ImplementationPlatform::GetCustomSavePath(const std::string& parent_folder,
                                                       const std::string& file_name) {
  return absl::StrCat(parent_folder, "/", file_name);
}

std::string ImplementationPlatform::GetDownloadPath(const std::string& parent_folder,
                                                    const std::string& file_name) {
  return absl::StrCat("/tmp/", file_name);
}

std::string ImplementationPlatform::GetDownloadPath(const std::string& file_name) {
  return absl::StrCat("/tmp/", file_name);
}

std::string ImplementationPlatform::GetAppDataPath(const std::string& file_name) {
  return absl::StrCat("/tmp/", file_name);
}

OSName ImplementationPlatform::GetCurrentOS() { return OSName::kLinux; }

std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(bool initial_value) {
  return std::make_unique<nearby::linux::AtomicBoolean>(initial_value);
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(std::uint32_t value) {
  return std::make_unique<nearby::linux::AtomicUint32>(value);
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(std::int32_t count) {
  return std::make_unique<shared::CountDownLatch>(count);
}

#pragma push_macro("CreateMutex")
#undef CreateMutex
std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  // Use the shared POSIX mutex implementation. The posix::Mutex is recursive by
  // design (uses PTHREAD_MUTEX_RECURSIVE), so return it for both regular and
  // recursive modes to use a consistent POSIX implementation across Linux.
  return std::make_unique<posix::Mutex>();
}
#pragma pop_macro("CreateMutex")

std::unique_ptr<ConditionVariable> ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  if (mutex == nullptr) return nullptr;
  // Expect a posix::Mutex instance here; if it's not, return nullptr.
  auto* derived = dynamic_cast<posix::Mutex*>(mutex);
  if (!derived) return nullptr;
  return std::make_unique<posix::ConditionVariable>(derived);
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(PayloadId, std::int64_t) {
  return nullptr;
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(const std::string&, size_t) {
  return nullptr;
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(PayloadId) {
  return nullptr;
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(const std::string&) {
  return nullptr;
}

std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(const char* file, int line, LogMessage::Severity severity) {
  return nullptr;
}

std::unique_ptr<SubmittableExecutor> ImplementationPlatform::CreateSingleThreadExecutor() {
  return std::make_unique<linux::MultiThreadExecutor>(1);
}

std::unique_ptr<SubmittableExecutor> ImplementationPlatform::CreateMultiThreadExecutor(std::int32_t max_concurrency) {
  return std::make_unique<linux::MultiThreadExecutor>(static_cast<int>(max_concurrency));
}

std::unique_ptr<ScheduledExecutor> ImplementationPlatform::CreateScheduledExecutor() {
  return std::unique_ptr<api::ScheduledExecutor>(new linux::ScheduledExecutor());
}

std::unique_ptr<AwdlMedium> ImplementationPlatform::CreateAwdlMedium() { return nullptr; }
std::unique_ptr<BluetoothAdapter> ImplementationPlatform::CreateBluetoothAdapter()
{
  static auto connection =  sdbus::createConnection();
  return std::make_unique<linux::BluetoothAdapter>(*connection, "/org/bluez/hci0");
}
std::unique_ptr<BluetoothClassicMedium> ImplementationPlatform::CreateBluetoothClassicMedium(BluetoothAdapter&) { return nullptr; }
std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(BluetoothAdapter&) { return nullptr; }
std::unique_ptr<api::ble_v2::BleMedium> ImplementationPlatform::CreateBleV2Medium(api::BluetoothAdapter&) { return nullptr; }
std::unique_ptr<api::CredentialStorage> ImplementationPlatform::CreateCredentialStorage() { return nullptr; }
std::unique_ptr<ServerSyncMedium> ImplementationPlatform::CreateServerSyncMedium() { return nullptr; }
std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() { return nullptr; }
std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() { return nullptr; }
std::unique_ptr<WifiHotspotMedium> ImplementationPlatform::CreateWifiHotspotMedium() { return nullptr; }
std::unique_ptr<WifiDirectMedium> ImplementationPlatform::CreateWifiDirectMedium() { return nullptr; }

std::unique_ptr<Timer> ImplementationPlatform::CreateTimer() { return nullptr; }

std::unique_ptr<DeviceInfo> ImplementationPlatform::CreateDeviceInfo() {
  return std::make_unique<linux::DeviceInfo>();
}

#ifndef NO_WEBRTC
std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() { return nullptr; }
#endif

absl::StatusOr<WebResponse> ImplementationPlatform::SendRequest(const WebRequest& request) {
  return absl::UnimplementedError("HTTP loader not implemented on this minimal linux platform");
}

#ifndef NEARBY_CHROMIUM
std::unique_ptr<nearby::api::PreferencesManager> ImplementationPlatform::CreatePreferencesManager(absl::string_view path) {
  return nullptr;
}
#endif

}  // namespace api
}  // namespace nearby
