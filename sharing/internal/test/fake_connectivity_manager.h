// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_CONNECTIVITY_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_CONNECTIVITY_MANAGER_H_

#include <functional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "sharing/internal/public/connectivity_manager.h"

namespace nearby {

class FakeConnectivityManager : public ConnectivityManager {
 public:
  bool IsLanConnected() override { return is_lan_connected_; }
  bool IsInternetConnected() override { return is_internet_connected_; }
  bool IsHPRealtekDevice() override { return is_hp_realtek_device_; }
  void SetIsHPRealtekDevice(bool is_hp_realtek_device) {
    is_hp_realtek_device_ = is_hp_realtek_device;
  }
  void RegisterLanListener(absl::string_view listener_name,
                           std::function<void(bool)> callback) override {
    lan_listeners_.emplace(listener_name, std::move(callback));
  }
  void UnregisterLanListener(absl::string_view listener_name) override {
    lan_listeners_.erase(listener_name);
  }
  void RegisterInternetListener(absl::string_view listener_name,
                                std::function<void(bool)> callback) override {
    internet_listeners_.emplace(listener_name, std::move(callback));
  }
  void UnregisterInternetListener(absl::string_view listener_name) override {
    internet_listeners_.erase(listener_name);
  }

  // Mocks connectivity methods.
  void SetLanConnected(bool connected) {
    is_lan_connected_ = connected;
    for (auto& listener : lan_listeners_) {
      listener.second(is_lan_connected_);
    }
  }

  void SetInternetConnected(bool connected) {
    is_internet_connected_ = connected;
    for (auto& listener : internet_listeners_) {
      listener.second(is_internet_connected_);
    }
  }

 private:
  bool is_lan_connected_ = true;
  bool is_internet_connected_ = true;
  bool is_hp_realtek_device_ = false;
  absl::flat_hash_map<std::string, std::function<void(bool)>> lan_listeners_;
  absl::flat_hash_map<std::string, std::function<void(bool)>>
      internet_listeners_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_CONNECTIVITY_MANAGER_H_
