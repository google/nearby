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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_WIFI_ADAPTER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_WIFI_ADAPTER_H_

#include <functional>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/base/observer_list.h"
#include "sharing/internal/api/wifi_adapter.h"

namespace nearby {

class FakeWifiAdapter : public sharing::api::WifiAdapter {
 public:
  FakeWifiAdapter() {
    num_present_received_ = 0;
    num_powered_received_ = 0;
  }

  ~FakeWifiAdapter() override = default;

  bool IsPresent() const override { return is_present_; }

  bool IsPowered() const override {
    // If the Wi-Fi adapter is not present, return false for power status.
    if (!is_present_) {
      return false;
    }

    return is_powered_;
  }

  sharing::api::WifiAdapter::PermissionStatus GetOsPermissionStatus()
      const override {
    return sharing::api::WifiAdapter::PermissionStatus::kAllowed;
  }

  void SetPowered(bool powered, std::function<void()> success_callback,
                  std::function<void()> error_callback) override {
    success_callback();
  }

  std::optional<std::string> GetAdapterId() const override { return "nearby"; }

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }
  bool HasObserver(Observer* observer) override {
    return observer_list_.HasObserver(observer);
  }

  // Mock OS Wi-Fi adapter presence state changed events
  void ReceivedAdapterPresentChangedFromOs(bool present) {
    num_present_received_ += 1;

    bool was_present = IsPresent();
    bool is_present = present;

    // Only trigger when state of presence changes values
    if (was_present != is_present) {
      is_present_ = is_present;
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->AdapterPresentChanged(this, is_present_);
        }
      }
    }
  }

  // Mock OS Wi-Fi adapter powered state changed events
  void ReceivedAdapterPoweredChangedFromOs(bool powered) {
    num_powered_received_ += 1;

    bool was_powered = IsPowered();
    bool is_powered = powered;

    // Only trigger when state of power changes values
    if (was_powered != is_powered) {
      is_powered_ = is_powered;
      for (Observer* observer : observer_list_.GetObservers()) {
        if (observer != nullptr) {
          observer->AdapterPoweredChanged(this, is_powered_);
        }
      }
    }
  }

  int GetNumPresentReceivedFromOS() { return num_present_received_; }
  int GetNumPoweredReceivedFromOS() { return num_powered_received_; }

 private:
  ObserverList<sharing::api::WifiAdapter::Observer> observer_list_;
  bool is_present_ = true;
  bool is_powered_ = true;
  int num_present_received_;
  int num_powered_received_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_WIFI_ADAPTER_H_
