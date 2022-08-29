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

#ifndef PLATFORM_API_PLATFORM_H_
#define PLATFORM_API_PLATFORM_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
#include "internal/platform/implementation/ble.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/bluetooth_adapter.h"
#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/condition_variable.h"
#include "internal/platform/implementation/count_down_latch.h"
#include "internal/platform/implementation/credential_storage.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/implementation/input_file.h"
#include "internal/platform/implementation/log_message.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/implementation/output_file.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/implementation/server_sync.h"
#include "internal/platform/implementation/settable_future.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/implementation/system_clock.h"
#ifndef NO_WEBRTC
#include "internal/platform/implementation/webrtc.h"
#endif
#include "internal/platform/implementation/wifi.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/os_name.h"
#include "internal/platform/payload_id.h"

namespace location {
namespace nearby {
namespace api {

// API rework notes:
// https://docs.google.com/spreadsheets/d/1erZNkX7pX8s5jWTHdxgjntxTMor3BGiY2H_fC_ldtoQ/edit#gid=381357998
class ImplementationPlatform {
 public:
  // General platform support:
  // - atomic variables (boolean, and any other copyable type)
  // - synchronization primitives:
  //   - mutex (regular, and recursive)
  //   - condition variable (must work with regular mutex only)
  //   - Future<T> : to synchronize on Callable<T> scheduled to execute.
  //   - CountDownLatch : to ensure at least N threads are waiting.
  // - file I/O
  // - Logging
  static std::string GetDownloadPath(absl::string_view parent_folder,
                                     absl::string_view file_name);

  static std::string GetDownloadPath(absl::string_view file_name);

  static std::string GetAppDataPath(absl::string_view file_name);

  static OSName GetCurrentOS();

  // Atomics:
  // =======

  // Atomic boolean: special case. Uses native platform atomics.
  // Does not use locking.
  // Does not use dynamic memory allocations in operations.
  static std::unique_ptr<AtomicBoolean> CreateAtomicBoolean(bool initial_value);

  // Supports enums and integers up to 32-bit.
  // Does not use locking, if platform supports 32-bit atimics natively.
  // Does not use dynamic memory allocations in operations.
  static std::unique_ptr<AtomicUint32> CreateAtomicUint32(std::uint32_t value);

  static std::unique_ptr<CountDownLatch> CreateCountDownLatch(
      std::int32_t count);
  static std::unique_ptr<Mutex> CreateMutex(Mutex::Mode mode);
  static std::unique_ptr<ConditionVariable> CreateConditionVariable(
      Mutex* mutex);

  static std::unique_ptr<InputFile> CreateInputFile(PayloadId, std::int64_t);

  static std::unique_ptr<InputFile> CreateInputFile(absl::string_view, size_t);

  static std::unique_ptr<OutputFile> CreateOutputFile(PayloadId);

  static std::unique_ptr<OutputFile> CreateOutputFile(absl::string_view);

  static std::unique_ptr<LogMessage> CreateLogMessage(
      const char* file, int line, LogMessage::Severity severity);

  // Java-like Executors
  static std::unique_ptr<SubmittableExecutor> CreateSingleThreadExecutor();
  static std::unique_ptr<SubmittableExecutor> CreateMultiThreadExecutor(
      std::int32_t max_concurrency);
  static std::unique_ptr<ScheduledExecutor> CreateScheduledExecutor();

  // Protocol implementations, domain-specific support
  static std::unique_ptr<BluetoothAdapter> CreateBluetoothAdapter();
  static std::unique_ptr<BluetoothClassicMedium> CreateBluetoothClassicMedium(
      BluetoothAdapter&);
  static std::unique_ptr<BleMedium> CreateBleMedium(BluetoothAdapter&);
  static std::unique_ptr<api::ble_v2::BleMedium> CreateBleV2Medium(
      api::BluetoothAdapter&);
  static std::unique_ptr<api::CredentialStorage> CreateCredentialStorage();
  static std::unique_ptr<ServerSyncMedium> CreateServerSyncMedium();
  static std::unique_ptr<WifiMedium> CreateWifiMedium();
  static std::unique_ptr<WifiLanMedium> CreateWifiLanMedium();
  static std::unique_ptr<WifiHotspotMedium> CreateWifiHotspotMedium();
#ifndef NO_WEBRTC
  static std::unique_ptr<WebRtcMedium> CreateWebRtcMedium();
#endif
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_PLATFORM_H_
