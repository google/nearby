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

#undef NDEBUG

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "spdlog/spdlog.h"
extern "C" {
#include "presence.h"
}

using namespace std;

const int EXPECTED_PRIORITY = 111;
const PresenceMedium EXPECTED_MEDIUM = PresenceMedium::BLE;
const int EXPECTED_ACTION_ZERO = 100;
const int EXPECTED_ACTION_ONE = 101;

struct PresenceEngine* engine;
PresenceBleScanRequest scan_request;

std::mutex scan_mutex;
std::condition_variable scan_notification;

std::mutex discovery_mutex;
std::condition_variable discovery_notification;

// BLE system API.
void start_ble_scan(PresenceBleScanRequest* request) {
  spdlog::info("Start BLE scan with Priority: {}", request->priority);
  for (auto action: request->actions) {
    spdlog::info("action: {}", action);
  }
  assert(request->priority == EXPECTED_PRIORITY);

  {
    std::unique_lock<std::mutex> lock(scan_mutex);
    scan_request = *request;
  }
  scan_notification.notify_all();
}

// Sends a BLE scan result in a separated thread.
thread platform_thread{[]() {
  while (true) {
    std::unique_lock<std::mutex> lock(scan_mutex);
    scan_notification.wait(lock);
    spdlog::info("Returns scan result.");
    auto scan_result_builder =
        presence_ble_scan_result_builder_new(PresenceMedium::BLE);
    for (auto action: scan_request.actions) {
      presence_ble_scan_result_builder_add_action(scan_result_builder, action);
    }
    auto scan_result =
        presence_ble_scan_result_builder_build(scan_result_builder);
    presence_on_scan_result(engine, scan_result);
    break;
  }
}};

// Client callback to receive discovery results.
void presence_discovery_callback(PresenceDiscoveryResult* result) {
  spdlog::info("Received discovery result with medium: {}, action: {}",
               (int) result->medium,
               result->device.actions[0]);
  for (auto action: result->device.actions) {
    spdlog::info("actions: {}", action);
  }
  assert(result->medium == EXPECTED_MEDIUM);
  assert(result->device.actions.size() == 2);
  assert(result->device.actions[0] == EXPECTED_ACTION_ZERO);
  assert(result->device.actions[1] == EXPECTED_ACTION_ONE);
  discovery_notification.notify_all();
}

int main(int argc, char** argv) {
  spdlog::info("Example to demo Presence Engine in Rust.");
  engine = presence_engine_new(presence_discovery_callback, start_ble_scan);

  thread engine_thread{[=]() { presence_engine_run(engine); }};

  auto request_builder = presence_request_builder_new(EXPECTED_PRIORITY);
  presence_request_builder_add_condition(request_builder,
                                         EXPECTED_ACTION_ZERO,
                                         PresenceIdentityType::Private,
                                         PresenceMeasurementAccuracy::CoarseAccuracy);
  presence_request_builder_add_condition(request_builder,
                                         EXPECTED_ACTION_ONE,
                                         PresenceIdentityType::Private,
                                         PresenceMeasurementAccuracy::CoarseAccuracy);
  auto request = presence_request_builder_build(request_builder);

  std::unique_lock<std::mutex> lock(discovery_mutex);
  presence_engine_set_discovery_request(engine, request);
  discovery_notification.wait(lock);

  presence_engine_stop(engine);
  engine_thread.join();
  platform_thread.join();
}