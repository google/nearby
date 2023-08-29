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

#include <algorithm>
#include <memory>

#include "connections/advertising_options.h"
#include "connections/core.h"
#include "connections/implementation/service_controller_router.h"
#include "connections/strategy.h"

using namespace nearby;
using namespace connections;

int main() {
  auto router = std::make_unique<ServiceControllerRouter>();
  auto core = Core(router.get());

  AdvertisingOptions a_options;
  a_options.strategy = Strategy::kP2pCluster;
  a_options.device_info = "connectionsd";
  a_options.enable_bluetooth_listening = true;
  a_options.low_power = false;
  a_options.allowed.ble = false;
  a_options.allowed.web_rtc = false;

  ConnectionRequestInfo con_req_info;
  con_req_info.listener.accepted_cb = [](const std::string &endpoint_id) {
    std::cout << "Accepted new endpoint: " << endpoint_id << std::endl;
  };

  core.StartAdvertising(
      "com.google.nearby.connectionsd", a_options.CompatibleOptions(),
      con_req_info, [](Status status) {
        std::cout << "Advertising result: " << status.ToString() << std::endl;
      });

  DiscoveryOptions d_options;
  d_options.strategy = Strategy::kP2pCluster;
  d_options.allowed.ble = false;
  d_options.allowed.web_rtc = false;

  DiscoveryListener d_listener;

  d_listener.endpoint_found_cb = [](const std::string &endpoint,
                                    const ByteArray &info,
                                    const std::string &service_id) {
    std::cout << "Found endpoint " << endpoint << " on service " << service_id;
  };
  core.StartDiscovery(
      "com.google.nearby.connectionsd", d_options.CompatibleOptions(),
      d_listener, [](Status status) {
        std::cout << "Discovery status: " << status.ToString() << std::endl;
      });

  while (true) {
    sleep(10);
  }
  return 0;
}
