// Copyright 2023 Google LLC
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

#include <filesystem>
#include <memory>
#include <string>

#include <curl/curl.h>
#include <sdbus-c++/Error.h>
#include <sdbus-c++/Types.h>

#include "device_info.h"
#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/count_down_latch.h"
#include "internal/platform/implementation/http_loader.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/linux/atomic_boolean.h"
#include "internal/platform/implementation/linux/atomic_uint32.h"
//#include "internal/platform/implementation/linux/ble_v2_medium.h"
#include "internal/platform/implementation/linux/bluetooth_adapter.h"
#include "internal/platform/implementation/linux/bluetooth_classic_medium.h"
#include "internal/platform/implementation/linux/bluez.h"
#include "internal/platform/implementation/linux/condition_variable.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/generated/dbus/bluez/adapter_client.h"
#include "internal/platform/implementation/linux/mutex.h"
#include "internal/platform/implementation/linux/preferences_manager.h"
#include "internal/platform/implementation/linux/submittable_executor.h"
#include "internal/platform/implementation/linux/timer.h"
// #include "internal/platform/implementation/linux/wifi_direct.h"
// #include "internal/platform/implementation/linux/wifi_hotspot.h"
#include "internal/platform/implementation/linux/wifi_lan.h"
// #include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/platform.h"

#include "ble_v2_medium.h"
#include "absl/strings/str_cat.h"

#include "internal/platform/implementation/shared/count_down_latch.h"
#include "internal/platform/implementation/shared/file.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/payload_id.h"
#include "scheduled_executor.h"
#include "wifi_direct.h"
#include "wifi_hotspot.h"

namespace nearby {
namespace api {
std::string ImplementationPlatform::GetCustomSavePath(
    const std::string &parent_folder, const std::string &file_name) {
  auto fs = std::filesystem::path(parent_folder);
  return (fs / file_name).string();
}

std::string ImplementationPlatform::GetDownloadPath(
    const std::string &parent_folder, const std::string &file_name) {
  std::filesystem::path downloads;
  const char* download_dir = getenv("XDG_DOWNLOAD_DIR");
  
  if (download_dir != nullptr) {
    downloads = std::filesystem::path(download_dir);
  } else {
    // Fallback to ~/Downloads if XDG_DOWNLOAD_DIR is not set
    const char* home = getenv("HOME");
    if (home != nullptr) {
      downloads = std::filesystem::path(home) / "Downloads";
    } else {
      downloads = "/tmp/Downloads";
    }
  }
  
  return (downloads / std::filesystem::path(parent_folder).filename() /
          std::filesystem::path(file_name).filename()).string();
}

std::string ImplementationPlatform::GetDownloadPath(
    const std::string &file_name) {
  std::filesystem::path downloads;
  const char* download_dir = getenv("XDG_DOWNLOAD_DIR");
  
  if (download_dir != nullptr) {
    downloads = std::filesystem::path(download_dir);
  } else {
    // Fallback to ~/Downloads if XDG_DOWNLOAD_DIR is not set
    const char* home = getenv("HOME");
    if (home != nullptr) {
      downloads = std::filesystem::path(home) / "Downloads";
    } else {
      downloads = "/tmp/Downloads";
    }
  }
  
  return (downloads / std::filesystem::path(file_name).filename()).string();
}

std::string ImplementationPlatform::GetAppDataPath(
    const std::string &file_name) {
  std::filesystem::path state;
  const char* state_home = getenv("XDG_STATE_HOME");
  
  if (state_home != nullptr) {
    state = std::filesystem::path(state_home);
  } else {
    // Fallback to ~/.local/state if XDG_STATE_HOME is not set
    const char* home = getenv("HOME");
    if (home != nullptr) {
      state = std::filesystem::path(home) / ".local" / "state";
    } else {
      state = "/tmp/state";
    }
  }
  
  return (state / std::filesystem::path(file_name).filename()).string();
}

OSName ImplementationPlatform::GetCurrentOS() { return OSName::kWindows; }

std::unique_ptr<api::AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(
    bool initial_value) {
  return std::make_unique<linux::AtomicBoolean>(initial_value);
}

std::unique_ptr<api::AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t value) {
  return std::make_unique<linux::AtomicUint32>(value);
}

std::unique_ptr<api::CountDownLatch>
ImplementationPlatform::CreateCountDownLatch(std::int32_t count) {
  return std::make_unique<shared::CountDownLatch>(count);
}

#pragma push_macro("CreateMutex")
#undef CreateMutex
std::unique_ptr<api::Mutex> ImplementationPlatform::CreateMutex(
    Mutex::Mode mode) {
  return std::make_unique<linux::Mutex>(mode);
}
#pragma pop_macro("CreateMutex")

std::unique_ptr<api::ConditionVariable>
ImplementationPlatform::CreateConditionVariable(api::Mutex *mutex) {
  return std::make_unique<linux::ConditionVariable>(mutex);
}

std::unique_ptr<api::InputFile> ImplementationPlatform::CreateInputFile(
    PayloadId id, std::int64_t total_size) {
  auto path = GetDownloadPath(std::to_string(id));
  return nearby::shared::IOFile::CreateInputFile(path, total_size);
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(
    const std::string &file_path, size_t size) {
  return nearby::shared::IOFile::CreateInputFile(file_path, size);
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    PayloadId payload_id) {
  return nearby::shared::IOFile::CreateOutputFile(
      GetDownloadPath("", std::to_string(payload_id)));
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(
    const std::string &file_path) {
  std::filesystem::path path(file_path);
  try {
    std::filesystem::create_directories(path.parent_path());
  } catch (std::filesystem::filesystem_error const &err) {
    LOG(ERROR) << __func__ << ": error creating directory tree "
                       << path.parent_path() << ": " << err.what();
  }

  return nearby::shared::IOFile::CreateOutputFile(path.string());
}

std::unique_ptr<api::LogMessage> ImplementationPlatform::CreateLogMessage(
    const char *file, int line, LogMessage::Severity severity) {
  return nullptr;
  // Disabled LogMessage
  // return std::make_unique<linux::LogMessage>(file, line, severity);
}

std::unique_ptr<api::SubmittableExecutor>
ImplementationPlatform::CreateSingleThreadExecutor() {
  return std::make_unique<linux::SubmittableExecutor>();
}

std::unique_ptr<api::SubmittableExecutor>
ImplementationPlatform::CreateMultiThreadExecutor(
    std::int32_t max_concurrency) {
  return std::make_unique<linux::SubmittableExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor>
ImplementationPlatform::CreateScheduledExecutor() {
  return std::make_unique<linux::ScheduledExecutor>();
}

std::unique_ptr<api::BluetoothAdapter>
ImplementationPlatform::CreateBluetoothAdapter() {
  auto system_bus = linux::getSystemBusConnection();
  auto manager = linux::bluez::BluezObjectManager(*system_bus);
  try {
    auto interfaces = manager.GetManagedObjects();
    for (auto &[object, properties] : interfaces) {
      if (properties.count(org::bluez::Adapter1_proxy::INTERFACE_NAME) == 1) {
        LOG(INFO) << __func__ << ": found bluetooth adapter " << object;
        return std::make_unique<linux::BluetoothAdapter>(system_bus, object);
      }
    }
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(&manager, "GetManagedObjects", e);
    return nullptr;
  }

  LOG(ERROR) << __func__
                     << ": couldn't find a bluetooth adapter on this system";
  return nullptr;
}

std::unique_ptr<api::BluetoothClassicMedium>
ImplementationPlatform::CreateBluetoothClassicMedium(
    BluetoothAdapter &adapter) {
  return std::make_unique<linux::BluetoothClassicMedium>(
      dynamic_cast<linux::BluetoothAdapter &>(adapter));
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(
    BluetoothAdapter &adapter) {
  return nullptr;
}

std::unique_ptr<api::ble_v2::BleMedium>
ImplementationPlatform::CreateBleV2Medium(api::BluetoothAdapter &adapter) {
  // return nullptr;
  // TODO: Enable BLEv2 once BlueZ support is added.
  return std::make_unique<linux::BleV2Medium>(
      dynamic_cast<linux::BluetoothAdapter &>(adapter));
}

namespace {
static std::unique_ptr<linux::NetworkManagerWifiMedium> createWifiMedium(
    std::shared_ptr<linux::networkmanager::NetworkManager> nm) {
  // return nullptr;
  std::vector<sdbus::ObjectPath> device_paths;

  try {
    device_paths = nm->GetAllDevices();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(nm, "GetAllDevices", e);
    return nullptr;
  }

  auto manager = linux::networkmanager::ObjectManager(nm->GetConnection());

  std::map<sdbus::ObjectPath,
           std::map<std::string, std::map<std::string, sdbus::Variant>>>
      objects;
  try {
    objects = manager.GetManagedObjects();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(nm, "GetManagedObjects", e);
    return nullptr;
  }

  for (auto &device_path : device_paths) {
    if (objects.count(device_path) == 1) {
      auto device = objects[device_path];
      if (device.count(org::freedesktop::NetworkManager::Device::
                           Wireless_proxy::INTERFACE_NAME) == 1) {
        LOG(INFO) << __func__
                          << ": Found a wireless device at :" << device_path;
        return std::make_unique<linux::NetworkManagerWifiMedium>(nm,
                                                                 device_path);
      }
    }
  }

  LOG(ERROR) << __func__
                     << ": couldn't find a wireless device on this system";
  return nullptr;
}
}  // namespace

std::unique_ptr<api::WifiMedium> ImplementationPlatform::CreateWifiMedium() {
  // return nullptr;
  auto nm =
      std::make_shared<linux::networkmanager::NetworkManager>(linux::getSystemBusConnection());
  return createWifiMedium(nm);
}

std::unique_ptr<api::WifiLanMedium>
ImplementationPlatform::CreateWifiLanMedium() {
  auto nm = std::make_shared<linux::networkmanager::NetworkManager>(
      linux::getSystemBusConnection());
  return std::make_unique<linux::WifiLanMedium>(nm);
}

std::unique_ptr<api::WifiHotspotMedium>
ImplementationPlatform::CreateWifiHotspotMedium() {
  auto nm =
      std::make_shared<linux::networkmanager::NetworkManager>(linux::getSystemBusConnection());
  auto wifiMedium = createWifiMedium(nm);

  if (wifiMedium == nullptr) {
    LOG(ERROR) << __func__ << ": Could not create a WiFi medium";
    return nullptr;
  }

  return std::make_unique<linux::NetworkManagerWifiHotspotMedium>(
      nm, std::move(wifiMedium));
}

std::unique_ptr<api::WifiDirectMedium>
ImplementationPlatform::CreateWifiDirectMedium() {
  // return nullptr;
  auto nm =
      std::make_shared<linux::networkmanager::NetworkManager>(linux::getSystemBusConnection());
  auto wifiMedium = createWifiMedium(nm);

  if (wifiMedium == nullptr) {
    LOG(ERROR) << __func__ << ": Could not create a WiFi medium";
    return nullptr;
  }

  return std::make_unique<linux::NetworkManagerWifiDirectMedium>(
      nm, std::move(wifiMedium));
}

std::unique_ptr<api::Timer> ImplementationPlatform::CreateTimer() {
  return std::make_unique<linux::Timer>();
}

std::unique_ptr<api::DeviceInfo> ImplementationPlatform::CreateDeviceInfo() {
  return std::make_unique<linux::DeviceInfo>(linux::getSystemBusConnection());
}

  std::unique_ptr<AwdlMedium> ImplementationPlatform::CreateAwdlMedium() {
  return nullptr;
}

absl::StatusOr<api::WebResponse> ImplementationPlatform::SendRequest(
    const WebRequest &request) {
  if (request.body.size() >= (8 * 1024 * 1024)) {
    return absl::Status(absl::StatusCode::kResourceExhausted,
                        "request body too large");
  }

  CURL *handle = curl_easy_init();
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = '\0';

  curl_easy_setopt(handle, CURLOPT_URL, request.url.c_str());
  curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, errbuf);

  if (request.method == "GET")
    curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
  else if (request.method == "POST")
    curl_easy_setopt(handle, CURLOPT_POST, 1L);
  else
    curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, request.method.c_str());

  curl_easy_setopt(handle, CURLOPT_UPLOAD, request.body.c_str());

  struct curl_slist *headers_slist = nullptr;

  for (auto &[key, value] : request.headers) {
    auto hdr = absl::StrCat(key, ": ", value);
    auto temp = curl_slist_append(headers_slist, hdr.c_str());
    if (temp == nullptr) {
      if (headers_slist != nullptr) {
        curl_slist_free_all(headers_slist);
      }
      return absl::Status(absl::StatusCode::kResourceExhausted,
                          "failed to append header to slist");
    }
  }

  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers_slist);

  api::WebResponse response;

  if (curl_easy_perform(handle) != CURLE_OK) {
    LOG(ERROR) << __func__
                       << ": Error performing HTTP request: " << errbuf;
    return absl::Status(absl::StatusCode::kUnknown, errbuf);
  }

  struct curl_header *prev = nullptr;
  struct curl_header *h;

  h = curl_easy_nextheader(handle, CURLH_HEADER, 0, prev);
  while (h != nullptr) {
    response.headers.emplace(h->name, h->value);
  }

  auto writefn = [](char *ptr, size_t size, size_t nmemb, void *userdata) {
    std::string *body = static_cast<std::string *>(userdata);
    body->append(ptr, size * nmemb);
  };

  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writefn);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA,
                   static_cast<void *>(&response.body));
  long status;
  curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
  response.status_code = status;
  return response;
}

#ifndef NEARBY_CHROMIUM
std::unique_ptr<nearby::api::PreferencesManager>
ImplementationPlatform::CreatePreferencesManager(absl::string_view path) {
  return std::make_unique<linux::PreferencesManager>(path);
}
#endif

}  // namespace api
}  // namespace nearby
