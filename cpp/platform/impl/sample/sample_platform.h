#ifndef PLATFORM_IMPL_SAMPLE_SAMPLE_PLATFORM_H_
#define PLATFORM_IMPL_SAMPLE_SAMPLE_PLATFORM_H_

#include <cstdint>

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
#include "platform/api/multi_thread_executor.h"
#include "platform/api/settable_future.h"
#include "platform/api/single_thread_executor.h"
#include "platform/api/system_clock.h"
#include "platform/api/thread_utils.h"
#include "platform/api/wifi.h"
#include "platform/cancelable.h"
#include "platform/impl/sample/sample_wifi_medium.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {
namespace sample {

// The SamplePlatform class below shows an example of the factory functions
// and typedefs.
class SamplePlatform {
 public:
  class SampleSubmittableExecutor
      : public SubmittableExecutor<SampleSubmittableExecutor> {
   public:
    template <typename T>
    Ptr<Future<T> > submit(Ptr<Callable<T> > callable) {
      return Ptr<Future<T> >();
    }
  };

  class SampleSingleThreadExecutor
      : public SingleThreadExecutor<SampleSubmittableExecutor> {
   public:
    void execute(Ptr<Runnable> runnable) override {}
    void shutdown() override {}
  };

  class SampleMultiThreadExecutor
      : public MultiThreadExecutor<SampleSubmittableExecutor> {
   public:
    void execute(Ptr<Runnable> runnable) override {}
    void shutdown() override {}
  };

  class SampleScheduledExecutor {
   public:
    Ptr<Cancelable> schedule(Ptr<Runnable> runnable,
                             std::int64_t delay_millis) {
      return Ptr<Cancelable>();
    }
    void shutdown() {}
  };

  typedef SampleSingleThreadExecutor SingleThreadExecutorType;
  static Ptr<SingleThreadExecutorType> createSingleThreadExecutor() {
    return MakePtr(new SingleThreadExecutorType());
  }

  typedef SampleMultiThreadExecutor MultiThreadExecutorType;
  static Ptr<MultiThreadExecutorType> createMultiThreadExecutor(
      std::int32_t max_concurrency) {
    return MakePtr(new MultiThreadExecutorType());
  }

  typedef SampleScheduledExecutor ScheduledExecutorType;
  static Ptr<ScheduledExecutorType> createScheduledExecutor() {
    return MakePtr(new ScheduledExecutorType());
  }

  static Ptr<BluetoothAdapter> createBluetoothAdapter() {
    return Ptr<BluetoothAdapter>();
  }

  static Ptr<WifiMedium> createWifiMedium() {
    return MakePtr(new SampleWifiMedium());
  }

  static Ptr<CountDownLatch> createCountDownLatch(std::int32_t count) {
    return Ptr<CountDownLatch>();
  }

  template <typename T>
  static Ptr<SettableFuture<T> > createSettableFuture() {
    return Ptr<SettableFuture<T> >();
  }

  static Ptr<ThreadUtils> createThreadUtils() { return Ptr<ThreadUtils>(); }

  static Ptr<SystemClock> createSystemClock() { return Ptr<SystemClock>(); }

  static Ptr<AtomicBoolean> createAtomicBoolean(bool initial_value) {
    return Ptr<AtomicBoolean>();
  }

  template <typename T>
  static Ptr<AtomicReference<T> > createAtomicReference(T initial_value) {
    return Ptr<AtomicReference<T> >();
  }

  static Ptr<BluetoothClassicMedium> createBluetoothClassicMedium() {
    return Ptr<BluetoothClassicMedium>();
  }

  static Ptr<BLEMedium> createBLEMedium() { return Ptr<BLEMedium>(); }

  static Ptr<BLEMediumV2> createBLEMediumV2() { return Ptr<BLEMediumV2>(); }

  static Ptr<Lock> createLock() { return Ptr<Lock>(); }

  static Ptr<ConditionVariable> createConditionVariable(Ptr<Lock> lock) {
    return Ptr<ConditionVariable>();
  }

  static Ptr<HashUtils> createHashUtils() { return Ptr<HashUtils>(); }

  static std::string getDeviceId() { return ""; }

  static std::string getPayloadPath(int64_t payload_id) {
    return "/tmp/" + std::to_string(payload_id);
  }
};

}  // namespace sample
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_SAMPLE_SAMPLE_PLATFORM_H_
