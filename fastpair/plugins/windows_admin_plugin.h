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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_PLUGINS_WINDOWS_ADMIN_PLUGIN_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_PLUGINS_WINDOWS_ADMIN_PLUGIN_H_

#include <memory>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/fast_pair_plugin.h"
#include "fastpair/fast_pair_seeker.h"
#include "fastpair/fast_pair_service.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

class WindowsAdminPlugin : public FastPairPlugin {
 public:
  struct PluginState {
    void DiscoveryClicked(DiscoveryAction action);
    void SetIsScreenLocked(bool is_locked);
    ObserverList<FastPairNotificationController::Observer> observers;
    const FastPairDevice* device = nullptr;
    std::unique_ptr<FastPairService> fast_pair_service;
    bool foreground_currently_showing_notification = false;
  };

  class Provider : public FastPairPluginProvider {
   public:
    explicit Provider(PluginState* state) : state_(state) {}
    std::unique_ptr<FastPairPlugin> GetPlugin(
        FastPairSeeker* seeker, const FastPairDevice* device) override {
      return std::make_unique<WindowsAdminPlugin>(seeker, device, state_);
    }

   private:
    PluginState* state_;
  };

  WindowsAdminPlugin(FastPairSeeker* seeker, const FastPairDevice* device,
                     PluginState* state)
      : seeker_(seeker), device_(device), state_(state) {}

  void OnInitialDiscoveryEvent(const InitialDiscoveryEvent& event) override;
  void OnPairEvent(const PairEvent& event) override;

 private:
  void NotifyShowNotification(const FastPairDevice& device);
  FastPairSeeker* seeker_;
  const FastPairDevice* device_;
  PluginState* state_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_PLUGINS_WINDOWS_ADMIN_PLUGIN_H_
