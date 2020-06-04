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

#ifndef PLATFORM_V2_BASE_MEDIUM_ENVIRONMENT_H_
#define PLATFORM_V2_BASE_MEDIUM_ENVIRONMENT_H_

#include <atomic>

#include "platform_v2/api/bluetooth_adapter.h"
#include "platform_v2/api/bluetooth_classic.h"
#include "platform_v2/base/listeners.h"
#include "platform_v2/public/single_thread_executor.h"
#include "absl/container/flat_hash_map.h"

namespace location {
namespace nearby {

// MediumEnvironment is a simulated environment which allows multiple instances
// of simulated HW devices to "work" together as if they are physical.
// For each medium type it provides necessary methods to implement
// advertising, discovery and establishment of a data link.
// NOTE: this code depends on public:types target.
class MediumEnvironment {
 public:
  using BluetoothDiscoveryCallback =
      api::BluetoothClassicMedium::DiscoveryCallback;
  MediumEnvironment(const MediumEnvironment&) = delete;
  MediumEnvironment& operator=(const MediumEnvironment&) = delete;

  // Creates and returns a reference to the global test environment instance.
  static MediumEnvironment& Instance();

  // Clears state. No notifications are sent.
  void Reset();

  // Waits for all previously scheduled jobs to finish.
  // This method works as a barrier that guarantees that after it returns, all
  // the activities that started before it was called, or while it was running
  // are ended. This means that system is at the state of relaxation when this
  // code returns. It requires external stimulus to get out of relaxation state.
  //
  // If enable_notifications is true (default), simulation environment
  // will send all future notification events to all registered objects,
  // whenever protocol requires that. This is expected behavior.
  // If enabled_notifications is false, future event notifications will not be
  // sent to registered instances. This is useful for protocol shutdown,
  // where we no longer care about notifications, and where notifications may
  // otherwise be delivered after the notification source or target lifeteme has
  // ended, and cause undefined behavior.
  void Sync(bool enable_notifications = true);

  // Adds an adapter to internal container.
  // Notify BluetoothClassicMediums if any that adapter state has changed.
  void OnBluetoothAdapterChangedState(api::BluetoothAdapter& adapter,
                                      api::BluetoothDevice& adapter_device,
                                      std::string name, bool enabled,
                                      api::BluetoothAdapter::ScanMode mode);

  // Adds medium-related info to allow for adapter discovery to work.
  // This provides acccess to this medium from other mediums, when protocol
  // expects they should communicate.
  void RegisterBluetoothMedium(api::BluetoothClassicMedium& medium,
                               api::BluetoothAdapter& medium_adapter);

  // Updates callback info to allow for dispatch of discovery events.
  //
  // Invokes callback asynchronously when any changes happen to discoverable
  // devices, or if the defice is turned off, whether or not it is discoverable,
  // if it was ever reported as discoverable.
  //
  // This should be called when discoverable state changes.
  // with user-specified callback when discovery is enabled, and with default
  // (empty) callback otherwise.
  void UpdateBluetoothMedium(api::BluetoothClassicMedium& medium,
                             BluetoothDiscoveryCallback callback);

  // Removes medium-related info. This should correspond to device power off.
  void UnregisterBluetoothMedium(api::BluetoothClassicMedium& medium);

 private:
  struct BluetoothMediumContext {
    BluetoothDiscoveryCallback callback;
    api::BluetoothAdapter* adapter = nullptr;
    // discovered device vs device name map.
    absl::flat_hash_map<api::BluetoothDevice*, std::string> devices;
  };

  // This is a singleton object, for which destructor will never be called.
  // Constructor will be invoked once from Instance() static method.
  // Object is create in-place (with a placement new) to guarantee that
  // destructor is not scheduled for execution at exit.
  MediumEnvironment() = default;
  ~MediumEnvironment() = default;

  void OnDeviceStateChanged(BluetoothMediumContext& info,
                            api::BluetoothDevice& device,
                            const std::string& name,
                            api::BluetoothAdapter::ScanMode mode, bool enabled);
  void RunOnMediumEnvironmentThread(std::function<void()> runnable);

  std::atomic_int job_count_ = 0;
  std::atomic_bool enable_notifications_ = false;
  SingleThreadExecutor executor_;

  // The following data members are accessed in the context of a private
  // executor_ thread.
  absl::flat_hash_map<api::BluetoothAdapter*, api::BluetoothDevice*>
      bluetooth_adapters_;
  absl::flat_hash_map<api::BluetoothClassicMedium*, BluetoothMediumContext>
      bluetooth_mediums_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_BASE_MEDIUM_ENVIRONMENT_H_
