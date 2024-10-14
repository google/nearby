// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_WIFI_ADAPTER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_WIFI_ADAPTER_H_

#include <functional>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "sharing/internal/api/wifi_adapter.h"

namespace nearby::sharing::api {

class MockWifiAdapter : public nearby::sharing::api::WifiAdapter {
 public:
  MockWifiAdapter() = default;
  MockWifiAdapter(const MockWifiAdapter&) = delete;
  MockWifiAdapter& operator=(const MockWifiAdapter&) = delete;
  ~MockWifiAdapter() override = default;

  MOCK_METHOD(bool, IsPresent, (), (const, override));
  MOCK_METHOD(bool, IsPowered, (), (const, override));
  MOCK_METHOD(void, SetPowered,
              (bool powered, std::function<void()> success_callback,
               std::function<void()> error_callback),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(bool, HasObserver, (Observer * observer), (override));
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_WIFI_ADAPTER_H_
