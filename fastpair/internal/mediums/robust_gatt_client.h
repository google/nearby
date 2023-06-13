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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_ROBUST_GATT_CLIENT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_ROBUST_GATT_CLIENT_H_

#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/single_thread_executor.h"
namespace nearby {
namespace fastpair {

// Gatt client on top of nearby::GattClient with higher level API and built-in
// retry mechanism. All methods are not blocking unless stated otherwise.
//
// Callbacks and timeouts.
// The blocking platform calls are running on a dedicated thread. The callbacks
// are called when the platform calls have completed. The
// timeouts define for how long we will retry the platform calls before giving
// up.
// Example 1:
// `connect_timeout` is 10 seconds, `BleV2Medium::ConnectToGattServer()` fails
// after 15 seconds. In this case, we will not retry connecting, the client
// ConnectionStatusCallback will be called after 15 seconds.
// Example 2:
// `connect_timeout` is 10 seconds, `BleV2Medium::ConnectToGattServer()` fails
// after 6 seconds each time. In this case, we will retry once, the client
// ConnectionStatusCallback will be called after 12 seconds (6 + 6).
// Example 3:
// `BleV2Medium::ConnectToGattServer()` never returns. The callback will not be
// called.
class RobustGattClient {
 public:
  using WriteCallback = absl::AnyInvocable<void(absl::Status result) &&>;
  using ReadCallback =
      absl::AnyInvocable<void(absl::StatusOr<absl::string_view> value) &&>;
  using NotifyCallback =
      absl::AnyInvocable<void(absl::StatusOr<absl::string_view> value)>;
  using ConnectionStatusCallback = absl::AnyInvocable<void(absl::Status)>;
  // Defines a characteristic on the server.
  struct UuidPair {
    // Preferred UUID, for example Fast Pair V1.
    Uuid primary_uuid;
    // Optional fallback UUID if the primary is not present on the server, for
    // example Fast Pair V0.
    Uuid fallback_uuid;
  };
  struct ConnectionParams {
    api::ble_v2::TxPowerLevel tx_power_level =
        api::ble_v2::TxPowerLevel::kUnknown;
    Uuid service_uuid;
    std::vector<UuidPair> characteristic_uuids;
    // Timeout for retrying connection attempts.
    absl::Duration connect_timeout = absl::Seconds(10);
    // Timeout for retrying service discovery.
    absl::Duration discovery_timeout = absl::Seconds(10);
    // Timeout for retrying read/write/subscribe operations.
    // Note, this timeout does not include the time needed to connect to the
    // gatt server and discover services.
    absl::Duration gatt_operation_timeout = absl::Seconds(15);
    // Exponential back off parameters. They describe how long we will wait
    // before retrying an operation.
    absl::Duration initial_back_off_step = absl::Milliseconds(100);
    absl::Duration max_back_off = absl::Seconds(3);
    float back_off_multiplier = 1.5;
  };

  // Creates the GATT client, connects to the server, and discovers
  // characteristics. The connection is established in the background. If the
  // connection is interrupted, we will try to reconnect. The caller does not
  // need to wait for connection before using the client. Instead, the caller
  // should create an instance of `RobustGattClient` and immediately start
  // calling other methods such as `CallRemoteFunction()`.
  RobustGattClient(BleV2Medium& medium, BleV2Peripheral peripheral,
                   const ConnectionParams& params,
                   ConnectionStatusCallback callback = nullptr)
      : medium_(medium),
        peripheral_(peripheral),
        params_(params),
        connection_status_callback_(std::move(callback)) {
    DCHECK_GT(params.initial_back_off_step, absl::ZeroDuration());
    DCHECK_GE(params.back_off_multiplier, 1.0);
    DCHECK_GE(params.max_back_off, params.initial_back_off_step);

    Connect();
  }

  // The destructor will block if there are any ongoing platform BLE blocking
  // calls.
  ~RobustGattClient();

  // Writes to the remote characteristic.
  //
  // `uuid_pair_index` is the index to `ConnectionParams::characteristic_uuids`.
  // `callback` is called asynchronously with the result of the write call. If
  // write fails with a status other than `absl::StatusCode::kUnavailable`, then
  // it is a permanent failure. All future calls will likely fail too.
  void WriteCharacteristic(int uuid_pair_index, absl::string_view value,
                           api::ble_v2::GattClient::WriteType write_type,
                           WriteCallback callback);

  // Performs write-and-response exchange.
  //
  // The call:
  // * subscribes to the characteristic notifications,
  // * writes to the remote characteristic,
  // * waits for the response via a gatt notify call,
  // * unsubsribes from the notification.
  //
  // `uuid_pair_index` is the index to `ConnectionParams::characteristic_uuids`.
  // `response` is called asynchronously with the remote server response. If
  // write fails with a status other than `absl::StatusCode::kUnavailable` or
  // `absl::StatusCode::kDeadlineExceeded`, then it is a permanent failure. All
  // future calls will likely fail too.
  // The timeouts accumulate. If the gatt client is still connecting to the
  // remote service, the call with wait for connection, discovery,
  // characteristic subscription, write operation and gatt notification. Each of
  // them have their own timeouts. Example: if connection takes 2 seconds,
  // discovery takes 5 seconds, subscribing to the characteristic takes 3,
  // writing to the characteristic takes 6 seconds but the provider never sends
  // a response, then the call will time out no sooner than 2s + 5s + 3s + 6s +
  // gatt_operation_timeout.
  void CallRemoteFunction(int uuid_pair_index, absl::string_view request,
                          NotifyCallback response);

  // Reads remote characteristic.
  //
  // `uuid_pair_index` is the index to `ConnectionParams::characteristic_uuids`.
  void ReadCharacteristic(int uuid_pair_index, ReadCallback callback);

  // Subscribes for remote characteristic updates.
  //
  // `uuid_pair_index` is the index to `ConnectionParams::characteristic_uuids`.
  // If `call_once` is true, then the callback will be automatically
  // unsubscribed after being called.
  //
  // Do not call `Subscribe()` or `Unsubscribe()` from the `callback`. It will
  // lock up.
  // Do not use `Subscribe()` and `CallRemoteFunction()` calls for the same
  // characteristic at the same time.
  void Subscribe(int uuid_pair_index, NotifyCallback callback,
                 bool call_once = false);

  // Unsubscribes from remote characteristic updates.
  //
  // `uuid_pair_index` is the index to `ConnectionParams::characteristic_uuids`.
  void Unsubscribe(int uuid_pair_index);

  // Disables the gatt client and disconnects from the gatt server if
  // connected. None of the callbacks will be triggered after `Stop()`, but if
  // a callback is currently running, it may continue running after `Stop()` has
  // returned.
  void Stop();

 private:
  class ExpBackOff {
   public:
    explicit ExpBackOff(const ConnectionParams& params)
        : back_off_step_(params.initial_back_off_step),
          multiplier_(params.back_off_multiplier),
          max_back_off_(params.max_back_off) {}
    absl::Duration NextBackOff() {
      absl::Duration result = back_off_;
      back_off_ += std::min(back_off_ + back_off_step_, max_back_off_);
      back_off_step_ *= multiplier_;
      return result;
    }

   private:
    absl::Duration back_off_step_;
    float multiplier_;
    absl::Duration max_back_off_;
    absl::Duration back_off_ = absl::ZeroDuration();
  };
  struct WriteRequest {
    int uuid_pair_index;
    std::string value;
    api::ble_v2::GattClient::WriteType write_type;
    WriteCallback callback;
    absl::Duration time_left;
    ExpBackOff back_off;
  };
  struct SubscribeRequest {
    int uuid_pair_index;
    absl::Duration time_left;
    ExpBackOff back_off;
  };
  struct ReadRequest {
    int uuid_pair_index;
    ReadCallback callback;
    absl::Duration time_left;
    ExpBackOff back_off;
  };

  using GattCharacteristic = api::ble_v2::GattCharacteristic;
  void Connect();
  absl::Status TryConnect() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_);
  absl::Status TryDiscoverServices() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_);
  std::vector<Uuid> GetPrimaryCharacteristicList();
  std::vector<Uuid> GetFallbackCharacteristicList();
  bool DiscoverServices(const Uuid& service_uuid,
                        const std::vector<Uuid>& characteristic_uuids)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_);
  std::optional<GattCharacteristic> GetCharacteristic(
      const Uuid& service_id, const UuidPair& characteristic_uuid_pair)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_);
  // May return nullptr.
  const GattCharacteristic* GetCharacteristic(int uuid_pair_index)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_);
  void NotifySubscriber(int uuid_pair_index,
                        absl::StatusOr<absl::string_view> value);
  void Cleanup() ABSL_EXCLUSIVE_LOCKS_REQUIRED(executor_);
  void Write(WriteRequest request);
  void Read(ReadRequest request);
  void Subscribe(SubscribeRequest request);
  void UnsubscribeInternal(int uuid_pair_index);
  bool HasSubsriberCallback(int uuid_pair_index);
  void NotifyClient(absl::Status status);
  void StartNotifyTimer(int uuid_pair_index);

  // A thread for running blocking tasks.
  SingleThreadExecutor executor_;
  Mutex mutex_;
  BleV2Medium& medium_;
  BleV2Peripheral peripheral_;
  ConnectionParams params_;
  ConnectionStatusCallback connection_status_callback_;
  std::unique_ptr<GattClient> gatt_client_ ABSL_GUARDED_BY(executor_);
  // Mapping from `uuid_pair_index` to subscription callbacks.
  // The entries are lazily initialized.
  absl::flat_hash_map<int, GattCharacteristic> characteristics_
      ABSL_GUARDED_BY(executor_);
  // Mapping from `uuid_pair_index` to subscription callbacks.
  struct NotifyCallbackInfo {
    NotifyCallback callback;
    // If `call_once` is true, then the callback will be called only once, and
    // then automatically unregistered.
    bool call_once;
    std::unique_ptr<TimerImpl> timer;
  };
  absl::flat_hash_map<int, NotifyCallbackInfo> notify_callbacks_
      ABSL_GUARDED_BY(mutex_);
  std::atomic_bool stopped_ = false;
  absl::Status status_ = absl::OkStatus();
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_ROBUST_GATT_CLIENT_H_
