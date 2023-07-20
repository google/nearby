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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_SERVICE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_SERVICE_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "fastpair/fast_pair_plugin.h"
#include "fastpair/fast_pair_seeker.h"
#include "fastpair/repository/fast_pair_device_repository.h"
#include "fastpair/server_access/fast_pair_client.h"
#include "fastpair/server_access/fast_pair_http_notifier.h"
#include "fastpair/repository/fast_pair_repository.h"
#include "internal/account/account_manager.h"
#include "internal/auth/authentication_manager.h"
#include "internal/network/http_client.h"
#include "internal/platform/device_info.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/task_runner.h"
#include "internal/preferences/preferences_manager.h"

namespace nearby {
namespace fastpair {

// Fast Pair Seeker Service. There should be only one instance of the service
// running on the system.
class FastPairService {
 public:
  FastPairService();
  // Constructor for tests. Allows us to inject a serverless metadata
  // repository.
  FastPairService(
      std::unique_ptr<auth::AuthenticationManager> authentication_manager,
      std::unique_ptr<network::HttpClient> http_client,
      std::unique_ptr<DeviceInfo> device_info);
  ~FastPairService();

  // Registers a plugin provider. `name` must be a unique.
  // Returns an error if a provider with the same `name` is already registered.
  absl::Status RegisterPluginProvider(
      absl::string_view name, std::unique_ptr<FastPairPluginProvider> provider);

  // Unregisters a plugin provider and removes all plugins created by that
  // provider. Returns an error if there is no plugin provider registered with
  // `name`.
  absl::Status UnregisterPluginProvider(absl::string_view name);

  // Returns a `FastPairSeeker` implementation. The returned object has the
  // same lifetime as this `FastPairService` instance.
  FastPairSeeker* GetSeeker() const { return seeker_.get(); }

  // Returns a `AccountManager` implementation. The returned object has the
  // same lifetime as this `FastPairService` instance.
  AccountManager* GetAccountManager() { return account_manager_.get(); }

  // Returns a `FastPairRepository` implementation. The returned object has the
  // same lifetime as this `FastPairService` instance.
  FastPairRepository* GetFastPairRepository() {
    return fast_pair_repository_.get();
  }

 private:
  struct PluginState {
    // Gets the plugin for `device`. Creates the plugin if it does not exist.
    FastPairPlugin* GetPlugin(FastPairSeeker* seeker,
                              const FastPairDevice* device);
    std::unique_ptr<FastPairPluginProvider> provider;
    absl::flat_hash_map<const FastPairDevice*, std::unique_ptr<FastPairPlugin>>
        plugins;
  };
  void OnInitialDiscoveryEvent(const FastPairDevice& device,
                               InitialDiscoveryEvent event);
  void OnSubsequentDiscoveryEvent(const FastPairDevice& device,
                                  SubsequentDiscoveryEvent event);
  void OnPairEvent(const FastPairDevice& device, PairEvent event);
  void OnScreenEvent(ScreenEvent event);
  void OnBatteryEvent(const FastPairDevice& device, BatteryEvent event);
  void OnRingEvent(const FastPairDevice& device, RingEvent event);
  void OnDeviceDestroyed(const FastPairDevice& device);

  SingleThreadExecutor executor_;
  FastPairHttpNotifier fast_pair_http_notifier_;
  std::unique_ptr<FastPairSeeker> seeker_;
  // Plugin name is the key.
  absl::flat_hash_map<std::string, PluginState> plugin_states_;
  FastPairDeviceRepository devices_{&executor_};
  std::unique_ptr<auth::AuthenticationManager> authentication_manager_;
  std::unique_ptr<network::HttpClient> http_client_;
  std::unique_ptr<DeviceInfo> device_info_;
  std::unique_ptr<TaskRunner> task_runner_;
  std::unique_ptr<preferences::PreferencesManager> preferences_manager_;
  std::unique_ptr<AccountManager> account_manager_;
  std::unique_ptr<FastPairClient> fast_pair_client_;
  std::unique_ptr<FastPairRepository> fast_pair_repository_;
  FastPairDeviceRepository::RemoveDeviceCallback on_device_destroyed_callback_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_FAST_PAIR_SERVICE_H_
