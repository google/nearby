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

  ConnectionType GetConnectionType() override { return connection_type_; }

  void RegisterConnectionListener(
      absl::string_view listener_name,
      std::function<void(ConnectionType, bool)> callback) override {
    listeners_.emplace(listener_name, std::move(callback));
  }
  void UnregisterConnectionListener(absl::string_view listener_name) override {
    listeners_.erase(listener_name);
  }

  // Mocks connectivity methods.
  void SetLanConnected(bool connected) {
    is_lan_connected_ = connected;
    for (auto& listener : listeners_) {
      listener.second(connection_type_, is_lan_connected_);
    }
  }

  // Mocks connectivity methods.
  void SetConnectionType(ConnectionType connection_type) {
    connection_type_ = connection_type;
    for (auto& listener : listeners_) {
      listener.second(connection_type_, is_lan_connected_);
    }
  }
  int GetListenerCount() const { return listeners_.size(); }

 private:
  bool is_lan_connected_ = true;
  ConnectionType connection_type_ = ConnectionType::kWifi;
  absl::flat_hash_map<std::string, std::function<void(ConnectionType, bool)>>
      listeners_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_CONNECTIVITY_MANAGER_H_
