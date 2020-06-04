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

#include "platform/api/atomic_boolean.h"
#include "platform/api/atomic_reference_def.h"
#include "platform/api/ble.h"
#include "platform/api/ble_v2.h"
#include "platform/api/bluetooth_adapter.h"
#include "platform/api/bluetooth_classic.h"
#include "platform/api/condition_variable.h"
#include "platform/api/count_down_latch.h"
#include "platform/api/hash_utils.h"
#include "platform/api/input_file.h"
#include "platform/api/lock.h"
#include "platform/api/output_file.h"
#include "platform/api/scheduled_executor.h"
#include "platform/api/server_sync.h"
#include "platform/api/settable_future_def.h"
#include "platform/api/submittable_executor_def.h"
#include "platform/api/system_clock.h"
#include "platform/api/thread_utils.h"
//#include "platform/api/webrtc.h"
#include "platform/api/wifi.h"
#include "platform/api/wifi_lan.h"

// Project-specific basic types, that are not part of API.
// TODO(apolyudov): replace with c++ standard types.
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "absl/types/any.h"

namespace location {
namespace nearby {
namespace platform {

// API rework notes:
// https://docs.google.com/spreadsheets/d/1erZNkX7pX8s5jWTHdxgjntxTMor3BGiY2H_fC_ldtoQ/edit#gid=381357998
class ImplementationPlatform {
 public:
  // Class Templates in platform code.
  //
  // Platform interface does not support templates directly.
  // This is a design decision. The purpose is to have type isolation
  // between platform library (or simply platform) and core library.
  // Another goal is to make a platform implementation a black box,
  // which does not leak implementation details in any form, be that types,
  // methods, or variables.
  //
  // Core library code does provide platform-specific class templates
  // on top of (a non-templated) platform support.
  //
  // For every common library template that needs platform support,
  // platform must provide an absl::any specialization of class template:
  template <typename T>
  static Ptr<AtomicReference<T>> createAtomicReference(T initial_value = T{});
  template <typename T>
  static Ptr<SettableFuture<T>> createSettableFuture();

  // AtomicReference<T>
  static Ptr<AtomicReference<absl::any>> createAtomicReferenceAny(
      absl::any initial_value);

  // SettableFuture<T>
  static Ptr<SettableFuture<absl::any>> createSettableFutureAny();

  // Non-template methods: general platform support.
  static Ptr<AtomicBoolean> createAtomicBoolean(bool initial_value);
  static Ptr<CountDownLatch> createCountDownLatch(std::int32_t count);
  static Ptr<Lock> createLock();
  static Ptr<ConditionVariable> createConditionVariable(Ptr<Lock> lock);
  static Ptr<HashUtils> createHashUtils();
  static Ptr<ThreadUtils> createThreadUtils();
  static Ptr<SystemClock> createSystemClock();
  static Ptr<InputFile> createInputFile(std::int64_t payload_id,
                                        std::int64_t total_size);
  static Ptr<OutputFile> createOutputFile(std::int64_t payload_id);

  // Java-like Executors
  // Type aliases used to API 1.0 compatibility.
  // They will be retired soon.
  // TODO(apolyudov): cleanup.
  using SingleThreadExecutorType = SubmittableExecutor;
  using MultiThreadExecutorType = SubmittableExecutor;
  using ScheduledExecutorType = ScheduledExecutor;

  static Ptr<SubmittableExecutor> createSingleThreadExecutor();
  static Ptr<SubmittableExecutor> createMultiThreadExecutor(
      std::int32_t max_concurrency);
  static Ptr<ScheduledExecutor> createScheduledExecutor();

  // Protocol implementations, domain-specific support
  static Ptr<BluetoothAdapter> createBluetoothAdapter();
  static Ptr<WifiMedium> createWifiMedium();
  static Ptr<BluetoothClassicMedium> createBluetoothClassicMedium();
  static Ptr<BLEMedium> createBLEMedium();
  static Ptr<BLEMediumV2> createBLEMediumV2();
  static Ptr<ServerSyncMedium> createServerSyncMedium();
  static Ptr<WifiLanMedium> createWifiLanMedium();
//  static Ptr<WebRtcSignalingMessenger> createWebRtcSignalingMessenger(
//      const std::string& self_id);
  static std::string getDeviceId();
};

}  // namespace platform
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_PLATFORM_H_
