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

#include "fastpair/fast_pair_service.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "fastpair/common/fast_pair_prefs.h"
#include "fastpair/fast_pair_plugin.h"
#include "fastpair/internal/fast_pair_seeker_impl.h"
#include "fastpair/repository/fast_pair_repository_impl.h"
#include "fastpair/server_access/fast_pair_client_impl.h"
#include "fastpair/server_access/fast_pair_http_notifier.h"
#include "internal/account/account_manager_impl.h"
#include "internal/auth/authentication_manager_impl.h"
#include "internal/flags/nearby_flags.h"
#include "internal/network/http_client_impl.h"
#include "internal/platform/device_info_impl.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/task_runner_impl.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr char kFastPairPreferencesFilePath[] = "Google/Nearby/FastPair";
constexpr FeatureFlags::Flags fast_pair_feature_flags = FeatureFlags::Flags{
    .enable_scan_for_fast_pair_advertisement = true,
    .skip_service_discovery_before_connecting_to_rfcomm = true,
};
constexpr absl::Duration kTimeout = absl::Seconds(3);
}  // namespace

FastPairService::FastPairService()
    : FastPairService(std::make_unique<auth::AuthenticationManagerImpl>(),
                      std::make_unique<network::NearbyHttpClient>(),
                      std::make_unique<DeviceInfoImpl>()) {}

FastPairService::FastPairService(
    std::unique_ptr<auth::AuthenticationManager> authentication_manager,
    std::unique_ptr<network::HttpClient> http_client,
    std::unique_ptr<DeviceInfo> device_info)
    : authentication_manager_(std::move(authentication_manager)),
      http_client_(std::move(http_client)),
      device_info_(std::move(device_info)),
      task_runner_(std::make_unique<TaskRunnerImpl>(1)),
      preferences_manager_(std::make_unique<preferences::PreferencesManager>(
          kFastPairPreferencesFilePath)),
      account_manager_(AccountManagerImpl::Factory::Create(
          preferences_manager_.get(), prefs::kNearbyFastPairUsersName,
          authentication_manager_.get(), task_runner_.get())),
      fast_pair_client_(std::make_unique<FastPairClientImpl>(
          authentication_manager_.get(), account_manager_.get(),
          http_client_.get(), &fast_pair_http_notifier_, device_info_.get())),
      fast_pair_repository_(
          std::make_unique<FastPairRepositoryImpl>(fast_pair_client_.get())),
      on_device_destroyed_callback_(
          [this](const FastPairDevice& device) { OnDeviceDestroyed(device); }) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableBleV2Gatt,
      true);
  const_cast<FeatureFlags&>(FeatureFlags::GetInstance())
      .SetFlags(fast_pair_feature_flags);

  devices_.AddObserver(&on_device_destroyed_callback_);
  seeker_ = std::make_unique<FastPairSeekerImpl>(
      FastPairSeekerImpl::ServiceCallbacks{
          .on_initial_discovery =
              [this](const FastPairDevice& device,
                     InitialDiscoveryEvent event) {
                OnInitialDiscoveryEvent(device, std::move(event));
              },
          .on_subsequent_discovery =
              [this](const FastPairDevice& device,
                     SubsequentDiscoveryEvent event) {
                OnSubsequentDiscoveryEvent(device, std::move(event));
              },
          .on_pair_event =
              [this](const FastPairDevice& device, PairEvent event) {
                OnPairEvent(device, std::move(event));
              },
          .on_screen_event =
              [this](ScreenEvent event) { OnScreenEvent(std::move(event)); },
          .on_battery_event =
              [this](const FastPairDevice& device, BatteryEvent event) {
                OnBatteryEvent(device, std::move(event));
              },
          .on_ring_event =
              [this](const FastPairDevice& device, RingEvent event) {
                OnRingEvent(device, std::move(event));
              }},
      &executor_, account_manager_.get(), &devices_,
      fast_pair_repository_.get());
}

FastPairService::~FastPairService() { executor_.Shutdown(); }

absl::Status FastPairService::RegisterPluginProvider(
    absl::string_view name, std::unique_ptr<FastPairPluginProvider> provider) {
  Future<absl::Status> result;
  executor_.Execute("register-plugin", [&]() {
    bool success = plugin_states_
                       .insert({std::string(name),
                                PluginState{.provider = std::move(provider)}})
                       .second;
    absl::Status status = success
                              ? absl::OkStatus()
                              : absl::AlreadyExistsError(absl::StrFormat(
                                    "Plugin '%s' already registered", name));
    result.Set(status);
  });
  ExceptionOr<absl::Status> status = result.Get(kTimeout);
  return status.ok() ? status.GetResult()
                     : absl::DeadlineExceededError("Register plugin timeout");
}

absl::Status FastPairService::UnregisterPluginProvider(absl::string_view name) {
  Future<absl::Status> result;
  executor_.Execute("unregister-plugin", [&]() {
    bool success = success = plugin_states_.erase(name);
    absl::Status status = success
                              ? absl::OkStatus()
                              : absl::NotFoundError(absl::StrFormat(
                                    "Plugin '%s' already registered", name));
    result.Set(status);
  });
  ExceptionOr<absl::Status> status = result.Get(kTimeout);
  return status.ok() ? status.GetResult()
                     : absl::DeadlineExceededError("Unregister plugin timeout");
}

void FastPairService::OnInitialDiscoveryEvent(const FastPairDevice& device,
                                              InitialDiscoveryEvent event) {
  executor_.Execute("on-initial-discovery", [this, device = &device,
                                             event = std::move(event)]() {
    NEARBY_LOGS(INFO) << "OnInitialDiscoveryEvent " << *device;
    for (auto& entry : plugin_states_) {
      auto plugin = entry.second.GetPlugin(seeker_.get(), device);
      plugin->OnInitialDiscoveryEvent(event);
    }
  });
}

void FastPairService::OnSubsequentDiscoveryEvent(
    const FastPairDevice& device, SubsequentDiscoveryEvent event) {}

void FastPairService::OnPairEvent(const FastPairDevice& device,
                                  PairEvent event) {
  executor_.Execute(
      "on-pair-event", [this, device = &device, event = std::move(event)]() {
        NEARBY_LOGS(INFO) << "OnPairEvent " << *device;
        for (auto& entry : plugin_states_) {
          auto plugin = entry.second.GetPlugin(seeker_.get(), device);
          plugin->OnPairEvent(event);
        }
      });
}

void FastPairService::OnScreenEvent(ScreenEvent event) {
  executor_.Execute("on-screen-event", [this, event = std::move(event)]() {
    NEARBY_LOGS(INFO) << "OnScreenEvent ";
    for (auto& entry : plugin_states_) {
      for (auto& plugin : entry.second.plugins) {
        plugin.second->OnScreenEvent(event);
      }
    }
  });
}
void FastPairService::OnBatteryEvent(const FastPairDevice& device,
                                     BatteryEvent event) {}
void FastPairService::OnRingEvent(const FastPairDevice& device,
                                  RingEvent event) {}

void FastPairService::OnDeviceDestroyed(const FastPairDevice& device) {
  NEARBY_LOGS(INFO) << "OnDeviceDestroyed " << device;
  for (auto& entry : plugin_states_) {
    entry.second.plugins.erase(&device);
  }
}

FastPairPlugin* FastPairService::PluginState::GetPlugin(
    FastPairSeeker* seeker, const FastPairDevice* device) {
  auto it = plugins.find(device);
  if (it != plugins.end()) {
    return it->second.get();
  }
  auto result = plugins.insert({device, provider->GetPlugin(seeker, device)});
  DCHECK(result.second);
  return result.first->second.get();
}
}  // namespace fastpair
}  // namespace nearby
