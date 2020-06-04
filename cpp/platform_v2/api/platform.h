#ifndef PLATFORM_V2_API_PLATFORM_H_
#define PLATFORM_V2_API_PLATFORM_H_

#include <cstdint>
#include <memory>
#include <string>

#include "platform_v2/api/atomic_boolean.h"
#include "platform_v2/api/atomic_reference.h"
#include "platform_v2/api/ble.h"
#include "platform_v2/api/ble_v2.h"
#include "platform_v2/api/bluetooth_adapter.h"
#include "platform_v2/api/bluetooth_classic.h"
#include "platform_v2/api/condition_variable.h"
#include "platform_v2/api/count_down_latch.h"
#include "platform_v2/api/crypto.h"
#include "platform_v2/api/input_file.h"
#include "platform_v2/api/mutex.h"
#include "platform_v2/api/output_file.h"
#include "platform_v2/api/scheduled_executor.h"
#include "platform_v2/api/server_sync.h"
#include "platform_v2/api/settable_future.h"
#include "platform_v2/api/submittable_executor.h"
#include "platform_v2/api/system_clock.h"
#include "platform_v2/api/webrtc.h"
#include "platform_v2/api/wifi.h"
#include "platform_v2/api/wifi_lan.h"
#include "absl/strings/string_view.h"
#include "absl/types/any.h"

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
  //   - Future<T> : to synchronize on Callable<T> schduled to execute.
  //   - CountDownLatch : to ensure at least N threads are waiting.
  // - file I/O
  static std::unique_ptr<AtomicReference<absl::any>> CreateAtomicReferenceAny(
      absl::any initial_value);
  static std::unique_ptr<SettableFuture<absl::any>> CreateSettableFutureAny();
  static std::unique_ptr<AtomicBoolean> CreateAtomicBoolean(bool initial_value);
  static std::unique_ptr<CountDownLatch> CreateCountDownLatch(
      std::int32_t count);
  static std::unique_ptr<Mutex> CreateMutex(Mutex::Mode mode);
  static std::unique_ptr<ConditionVariable> CreateConditionVariable(
      Mutex* mutex);
  static std::unique_ptr<InputFile> CreateInputFile(std::int64_t payload_id,
                                                    std::int64_t total_size);
  static std::unique_ptr<OutputFile> CreateOutputFile(std::int64_t payload_id);

  // Java-like Executors
  static std::unique_ptr<SubmittableExecutor> CreateSingleThreadExecutor();
  static std::unique_ptr<SubmittableExecutor> CreateMultiThreadExecutor(
      std::int32_t max_concurrency);
  static std::unique_ptr<ScheduledExecutor> CreateScheduledExecutor();

  // Protocol implementations, domain-specific support
  static std::unique_ptr<BluetoothAdapter> CreateBluetoothAdapter();
  static std::unique_ptr<BluetoothClassicMedium> CreateBluetoothClassicMedium();
  static std::unique_ptr<BleMedium> CreateBleMedium();
  static std::unique_ptr<ble_v2::BleMedium> CreateBleV2Medium();
  static std::unique_ptr<ServerSyncMedium> CreateServerSyncMedium();
  static std::unique_ptr<WifiMedium> CreateWifiMedium();
  static std::unique_ptr<WifiLanMedium> CreateWifiLanMedium();
  static std::unique_ptr<WebRtcMedium> CreateWebRtcMedium();
  static std::string GetDeviceId();
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_API_PLATFORM_H_
