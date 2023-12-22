// Copyright 2022 Google LLC
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

#ifndef LOCATION_NEARBY_CPP_SHARING_IMPLEMENTATION_INTERNAL_TEST_FAKE_BLUETOOTH_ADAPTER_OBSERVER_H_
#define LOCATION_NEARBY_CPP_SHARING_IMPLEMENTATION_INTERNAL_TEST_FAKE_BLUETOOTH_ADAPTER_OBSERVER_H_

#include "sharing/internal/api/bluetooth_adapter.h"

namespace nearby {

class FakeBluetoothAdapterObserver
    : public sharing::api::BluetoothAdapter::Observer {
 public:
  explicit FakeBluetoothAdapterObserver(
      sharing::api::BluetoothAdapter* adapter) {
    adapter_ = adapter;
    num_adapter_present_changed_ = 0;
    num_adapter_powered_changed_ = 0;
  }

  void AdapterPresentChanged(sharing::api::BluetoothAdapter* adapter,
                             bool present) override {
    if (adapter == adapter_) {
      observed_present_value_ = present;
      num_adapter_present_changed_ += 1;
    }
  }

  void AdapterPoweredChanged(sharing::api::BluetoothAdapter* adapter,
                             bool powered) override {
    if (adapter == adapter_) {
      observed_powered_value_ = powered;
      num_adapter_powered_changed_ += 1;
    }
  }
  bool GetObservedPresentValue() { return observed_present_value_; }
  bool GetObservedPoweredValue() { return observed_powered_value_; }

  int GetNumAdapterPresentChanged() { return num_adapter_present_changed_; }
  int GetNumAdapterPoweredChanged() { return num_adapter_powered_changed_; }

 private:
  sharing::api::BluetoothAdapter* adapter_;
  bool observed_present_value_ = true;
  bool observed_powered_value_ = true;
  int num_adapter_present_changed_;
  int num_adapter_powered_changed_;
};

}  // namespace nearby

#endif  // LOCATION_NEARBY_CPP_SHARING_IMPLEMENTATION_INTERNAL_TEST_FAKE_BLUETOOTH_ADAPTER_OBSERVER_H_
