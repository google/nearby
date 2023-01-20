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

#include <windows.h>

#include <cstdint>
#include <string>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "connections/advertising_options.h"
#include "connections/core.h"
#include "connections/implementation/service_controller_router.h"
#include "connections/listeners.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"

namespace nearby::windows {

using ::nearby::ByteArray;
using ::nearby::connections::AdvertisingOptions;
using ::nearby::connections::ConnectionListener;
using ::nearby::connections::ConnectionRequestInfo;
using ::nearby::connections::Core;
using ::nearby::connections::ServiceControllerRouter;
using ::nearby::connections::Status;
using ::nearby::connections::Strategy;

constexpr absl::string_view SERVICE_ID =
    "com.google.location.nearby.apps.helloconnections";

constexpr int TimeoutSeconds = 3;
constexpr int LoopCount = 10;
constexpr absl::string_view device_name = "12345678901";

AdvertisingOptions AdvertiseOptions{
    {
        // Strategy
        {
            Strategy::kP2pPointToPoint,
        },
        // Allowed:
        {
            true,  // bluetooth
            true,  // ble
            true,  // webrtc
            true,  // wifi_lan
            true,  // wifi_hotspot
        },
    },
    true,   // auto_upgrade_bandwidth
    true,   // enforce_topology_constraints
    false,  // low_power
    false,  // enable_bluetooth_listening
    false,  // enable_webrtc_listening
    false,  // is_out_of_band_connection
    "",     // fast_advertisement_service_uuid
};

class PerformanceTimer {
 public:
  static void start() {
    QueryPerformanceFrequency(&frequency_);
    QueryPerformanceCounter(&starting_time_);
  }
  static void stop() {
    QueryPerformanceCounter(&ending_time_);
    elapsed_microseconds_.QuadPart =
        ending_time_.QuadPart - starting_time_.QuadPart;
    elapsed_milliseconds_ = elapsed_microseconds_.QuadPart / 100;
  }

  static uint64_t ElapsedMilliseconds() { return elapsed_milliseconds_; }

 private:
  static uint64_t elapsed_milliseconds_;
  static LARGE_INTEGER starting_time_;
  static LARGE_INTEGER ending_time_;
  static LARGE_INTEGER elapsed_microseconds_;
  static LARGE_INTEGER frequency_;
};

uint64_t PerformanceTimer::elapsed_milliseconds_;
LARGE_INTEGER PerformanceTimer::starting_time_;
LARGE_INTEGER PerformanceTimer::ending_time_;
LARGE_INTEGER PerformanceTimer::elapsed_microseconds_;
LARGE_INTEGER PerformanceTimer::frequency_;

TEST(BluetoothClassicServerSocketTest,
     DISABLED_SingleRunWithTimeoutReproStuck) {
  ServiceControllerRouter router;
  Core core(&router);

  ConnectionListener listener;

  ConnectionRequestInfo request_info;

  request_info.endpoint_info = ByteArray(std::string(device_name));
  request_info.listener = listener;

  Status request_result;
  absl::Notification notification;

  PerformanceTimer::start();

  core.StartAdvertising(SERVICE_ID, AdvertiseOptions, request_info,
                        {.result_cb = [&](Status status) {
                          request_result = status;
                          notification.Notify();
                        }});

  if (notification.WaitForNotificationWithTimeout(
          absl::Seconds(TimeoutSeconds))) {
    PerformanceTimer::stop();

    NEARBY_LOGS(INFO) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                      << " Started advertising elapsed time: "
                      << std::to_string(
                             PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
    EXPECT_TRUE(false) << "Started advertising elapsed time: "
                       << std::to_string(ElapsedMilliseconds);
#endif

    std::cout << "StartAdvertising started once:" << request_result.ToString()
              << std::endl;
    NEARBY_LOGS(INFO) << "StartAdvertising started once:"
                      << request_result.ToString();
  } else {
    PerformanceTimer::stop();

    NEARBY_LOGS(INFO) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                      << " Timeout on starting advertising elapsed time: "
                      << std::to_string(
                             PerformanceTimer::ElapsedMilliseconds());

    EXPECT_TRUE(false) << "Timeout on starting advertising elapsed time: "
                       << std::to_string(
                              PerformanceTimer::ElapsedMilliseconds());

    std::cout << "StartAdvertising failed to start once:"
              << request_result.ToString() << std::endl;
    NEARBY_LOGS(INFO) << "StartAdvertising failed to start once:"
                      << request_result.ToString();
  }

  absl::Notification notification2;

  PerformanceTimer::start();

  core.StartAdvertising(SERVICE_ID, AdvertiseOptions, request_info,
                        {.result_cb = [&](Status status) {
                          request_result = status;
                          notification2.Notify();
                        }});

  if (notification2.WaitForNotificationWithTimeout(
          absl::Seconds(TimeoutSeconds))) {
    PerformanceTimer::stop();

    NEARBY_LOGS(INFO) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                      << " Started advertising second time elapsed time: "
                      << std::to_string(
                             PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
    EXPECT_TRUE(false) << "Started advertising elapsed time: "
                       << std::to_string(ElapsedMilliseconds);
#endif

    std::cout << "StartAdvertising started twice:" << request_result.ToString()
              << std::endl;
    NEARBY_LOGS(INFO) << "StartAdvertising started twice:"
                      << request_result.ToString();
  } else {
    PerformanceTimer::stop();

    NEARBY_LOGS(INFO)
        << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
        << " Timeout on starting advertising the second time elapsed time: "
        << std::to_string(PerformanceTimer::ElapsedMilliseconds());

    EXPECT_TRUE(false) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                       << " Timeout for started advertising elapsed time: "
                       << std::to_string(
                              PerformanceTimer::ElapsedMilliseconds());

    std::cout << "StartAdvertising failed to start twice:"
              << request_result.ToString() << std::endl;
    NEARBY_LOGS(INFO) << "StartAdvertising failed to start twice:"
                      << request_result.ToString();
  }

  absl::Notification notification3;

  PerformanceTimer::start();

  core.StopAdvertising({.result_cb = [&](Status status) {
    request_result = status;
    notification3.Notify();
  }});

  if (notification3.WaitForNotificationWithTimeout(
          absl::Seconds(TimeoutSeconds))) {
    PerformanceTimer::stop();

    NEARBY_LOGS(INFO) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                      << " Stopped advertising first time elapsed time: "
                      << std::to_string(
                             PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
    EXPECT_TRUE(false) << "Started advertising elapsed time: "
                       << std::to_string(elapsed_microseconds);
#endif

    std::cout << "StopAdvertising called once:" << request_result.ToString()
              << std::endl;
    NEARBY_LOGS(INFO) << "StopAdvertising called once:"
                      << request_result.ToString();
  } else {
    PerformanceTimer::stop();

    NEARBY_LOGS(INFO)
        << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
        << " Timeout on stopping advertising the first time elapsed time: "
        << std::to_string(PerformanceTimer::ElapsedMilliseconds());

    EXPECT_TRUE(false) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                       << " Timeout on stop advertising elapsed time: "
                       << std::to_string(
                              PerformanceTimer::ElapsedMilliseconds());

    std::cout << "StopAdvertising failed to stop once:"
              << request_result.ToString() << std::endl;
    NEARBY_LOGS(INFO) << "StopAdvertising failed to stop  once:"
                      << request_result.ToString();
  }

  std::cout << "Test completed." << std::endl;
  NEARBY_LOGS(INFO) << "Test completed.";
}

TEST(BluetoothClassicServerSocketTest, DISABLED_MultiRunWithTimeoutReproStuck) {
  ServiceControllerRouter router;
  Core core(&router);

  ConnectionListener listener;

  ConnectionRequestInfo request_info;

  request_info.endpoint_info = ByteArray(std::string(device_name));
  request_info.listener = listener;

  for (int loop_count = 0; loop_count < LoopCount; ++loop_count) {
    Status request_result;
    absl::Notification notification;

    PerformanceTimer::start();

    core.StartAdvertising(SERVICE_ID, AdvertiseOptions, request_info,
                          {.result_cb = [&](Status status) {
                            request_result = status;
                            notification.Notify();
                          }});

    if (notification.WaitForNotificationWithTimeout(
            absl::Seconds(TimeoutSeconds))) {
      PerformanceTimer::stop();

      NEARBY_LOGS(INFO) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                        << " Started advertising elapsed time: "
                        << std::to_string(
                               PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
      EXPECT_TRUE(false) << "Started advertising elapsed time : "
                         << std::to_string(ElapsedMilliseconds);
#endif

      std::cout << "StartAdvertising started once:" << request_result.ToString()
                << std::endl;
      NEARBY_LOGS(INFO) << "StartAdvertising started once:"
                        << request_result.ToString();
    } else {
      PerformanceTimer::stop();

      NEARBY_LOGS(INFO) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                        << " Timeout on starting advertising elapsed time: "
                        << std::to_string(
                               PerformanceTimer::ElapsedMilliseconds());

      EXPECT_TRUE(false) << "Timeout on starting advertising elapsed time: "
                         << std::to_string(
                                PerformanceTimer::ElapsedMilliseconds());

      std::cout << "StartAdvertising failed to start once:"
                << request_result.ToString() << std::endl;
      NEARBY_LOGS(INFO) << "StartAdvertising failed to start once:"
                        << request_result.ToString();
    }

    absl::Notification notification2;

    PerformanceTimer::start();

    core.StartAdvertising(SERVICE_ID, AdvertiseOptions, request_info,
                          {.result_cb = [&](Status status) {
                            request_result = status;
                            notification2.Notify();
                          }});

    if (notification2.WaitForNotificationWithTimeout(
            absl::Seconds(TimeoutSeconds))) {
      PerformanceTimer::stop();

      NEARBY_LOGS(INFO) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                        << " Started advertising second time elapsed time: "
                        << std::to_string(
                               PerformanceTimer::ElapsedMilliseconds());
      // EXPECT_TRUE(false)
      //     << "Started advertising elapsed time: "
      //     << std::to_string(ElapsedMilliseconds);
      std::cout << "StartAdvertising started twice:"
                << request_result.ToString() << std::endl;
      NEARBY_LOGS(INFO) << "StartAdvertising started twice:"
                        << request_result.ToString();
    } else {
      PerformanceTimer::stop();

      NEARBY_LOGS(INFO)
          << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
          << " Timeout on starting advertising the second time elapsed time: "
          << std::to_string(PerformanceTimer::ElapsedMilliseconds());

      EXPECT_TRUE(false)
          << "Timeout on starting advertising the second time elapsed time: "
          << std::to_string(PerformanceTimer::ElapsedMilliseconds());

      std::cout << "StartAdvertising failed to start twice:"
                << request_result.ToString() << std::endl;
      NEARBY_LOGS(INFO) << "StartAdvertising failed to start twice:"
                        << request_result.ToString();
    }

    absl::Notification notification3;

    PerformanceTimer::start();

    core.StopAdvertising({.result_cb = [&](Status status) {
      request_result = status;
      notification3.Notify();
    }});

    if (notification3.WaitForNotificationWithTimeout(
            absl::Seconds(TimeoutSeconds))) {
      PerformanceTimer::stop();

      NEARBY_LOGS(INFO) << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
                        << " Stopped advertising first time elapsed time: "
                        << std::to_string(
                               PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
      EXPECT_TRUE(false) << "Started advertising elapsed time: "
                         << std::to_string(ElapsedMilliseconds);
#endif

      std::cout << "StopAdvertising called once:" << request_result.ToString()
                << std::endl;
      NEARBY_LOGS(INFO) << "StopAdvertising called once:"
                        << request_result.ToString();
    } else {
      PerformanceTimer::stop();

      NEARBY_LOGS(INFO)
          << "SingleRunWithTimeoutReproStuck Line: " << __LINE__
          << " Timeout on stopping advertising the first time elapsed time: "
          << std::to_string(PerformanceTimer::ElapsedMilliseconds());

      EXPECT_TRUE(false) << "Timeout on started advertising elapsed time: "
                         << std::to_string(
                                PerformanceTimer::ElapsedMilliseconds());

      std::cout << "StopAdvertising failed to stop once:"
                << request_result.ToString() << std::endl;
      NEARBY_LOGS(INFO) << "StopAdvertising failed to stop  once:"
                        << request_result.ToString();
    }
  }

  std::cout << "Test completed." << std::endl;
  NEARBY_LOGS(INFO) << "Test completed.";
}

TEST(BluetoothClassicServerSocketTest, DISABLED_SingleRunNoTimeoutReproStuck) {
  ServiceControllerRouter router;
  Core core(&router);

  ConnectionListener listener;

  ConnectionRequestInfo request_info;

  request_info.endpoint_info = ByteArray(std::string(device_name));
  request_info.listener = listener;

  Status request_result;
  absl::Notification notification;

  PerformanceTimer::start();

  core.StartAdvertising(SERVICE_ID, AdvertiseOptions, request_info,
                        {.result_cb = [&](Status status) {
                          request_result = status;
                          notification.Notify();
                        }});

  notification.WaitForNotification();
  PerformanceTimer::stop();

  NEARBY_LOGS(INFO) << "SingleRunNoTimeoutReproStuck Line: " << __LINE__
                    << " Started advertising first time elapsed time: "
                    << std::to_string(PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
  EXPECT_TRUE(false) << "Started advertising first time elapsed time: "
                     << std::to_string(ElapsedMilliseconds);
#endif

  std::cout << "StartAdvertising started once:" << request_result.ToString()
            << std::endl;
  NEARBY_LOGS(INFO) << "StartAdvertising started once:"
                    << request_result.ToString();

  absl::Notification notification2;

  PerformanceTimer::start();

  core.StartAdvertising(SERVICE_ID, AdvertiseOptions, request_info,
                        {.result_cb = [&](Status status) {
                          request_result = status;
                          notification2.Notify();
                        }});

  notification2.WaitForNotification();
  PerformanceTimer::stop();

  NEARBY_LOGS(INFO) << "SingleRunNoTimeoutReproStuck Line: " << __LINE__
                    << " Started advertising first time elapsed time: "
                    << std::to_string(PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
  EXPECT_TRUE(false) << "Started advertising first time elapsed time: "
                     << std::to_string(ElapsedMilliseconds);
#endif

  std::cout << "StartAdvertising started twice:" << request_result.ToString()
            << std::endl;
  NEARBY_LOGS(INFO) << "StartAdvertising started twice:"
                    << request_result.ToString();

  absl::Notification notification3;

  PerformanceTimer::start();

  core.StopAdvertising({.result_cb = [&](Status status) {
    request_result = status;
    notification3.Notify();
  }});

  notification3.WaitForNotification();
  PerformanceTimer::stop();

  NEARBY_LOGS(INFO) << "SingleRunNoTimeoutReproStuck " << __LINE__
                    << "Stopped advertising elapsed time: "
                    << std::to_string(PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
  EXPECT_TRUE(false) << "Stopped advertising elapsed time: "
                     << std::to_string(ElapsedMilliseconds);
#endif

  std::cout << "StopAdvertising called once:" << request_result.ToString()
            << std::endl;
  NEARBY_LOGS(INFO) << "StopAdvertising called once:"
                    << request_result.ToString();

  std::cout << "Test completed." << std::endl;
  NEARBY_LOGS(INFO) << "Test completed.";
}

TEST(BluetoothClassicServerSocketTest, DISABLED_MultiRunNoTimeoutReproStuck) {
  ServiceControllerRouter router;
  Core core(&router);

  ConnectionListener listener;

  ConnectionRequestInfo request_info;

  request_info.endpoint_info = ByteArray(std::string(device_name));
  request_info.listener = listener;

  for (int loop_count = 0; loop_count < LoopCount; ++loop_count) {
    Status request_result;
    absl::Notification notification;

    PerformanceTimer::start();

    core.StartAdvertising(SERVICE_ID, AdvertiseOptions, request_info,
                          {.result_cb = [&](Status status) {
                            request_result = status;
                            notification.Notify();
                          }});

    notification.WaitForNotification();

    PerformanceTimer::stop();

    NEARBY_LOGS(INFO) << "MultiRunNoTimeoutReproStuck " << __LINE__
                      << "Started advertising elapsed time: "
                      << std::to_string(
                             PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
    EXPECT_TRUE(false) << "Started advertising elapsed time: "
                       << std::to_string(ElapsedMilliseconds);
#endif

    std::cout << "MultiRunNoTimeoutReproStuck " << __LINE__
              << " : StartAdvertising started once: "
              << request_result.ToString() << std::endl;
    NEARBY_LOGS(INFO) << "StartAdvertising started once:"
                      << request_result.ToString();

    absl::Notification notification2;

    PerformanceTimer::start();

    core.StartAdvertising(SERVICE_ID, AdvertiseOptions, request_info,
                          {.result_cb = [&](Status status) {
                            request_result = status;
                            notification2.Notify();
                          }});

    notification2.WaitForNotification();
    PerformanceTimer::stop();

    NEARBY_LOGS(INFO) << "MultiRunNoTimeoutReproStuck " << __LINE__
                      << "Started advertising elapsed time: "
                      << std::to_string(
                             PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
    EXPECT_TRUE(false) << "Started advertising elapsed time: "
                       << std::to_string(ElapsedMilliseconds);
#endif

    std::cout << "MultiRunNoTimeoutReproStuck " << __LINE__
              << "StartAdvertising started twice:" << request_result.ToString()
              << std::endl;
    NEARBY_LOGS(INFO) << "MultiRunNoTimeoutReproStuck " << __LINE__
                      << "StartAdvertising started twice:"
                      << request_result.ToString();

    absl::Notification notification3;

    PerformanceTimer::start();

    core.StopAdvertising({.result_cb = [&](Status status) {
      request_result = status;
      notification3.Notify();
    }});

    notification3.WaitForNotification();
    PerformanceTimer::stop();

    NEARBY_LOGS(INFO) << "MultiRunNoTimeoutReproStuck " << __LINE__
                      << "Stopped advertising elapsed time: "
                      << std::to_string(
                             PerformanceTimer::ElapsedMilliseconds());

#ifdef TEST_OUTPUT
    EXPECT_TRUE(false) << "Stop advertising elapsed time: "
                       << std::to_string(ElapsedMilliseconds);
#endif

    std::cout << "StopAdvertising called once:" << request_result.ToString()
              << std::endl;
    NEARBY_LOGS(INFO) << "MultiRunNoTimeoutReproStuck " << __LINE__
                      << "StopAdvertising called once:"
                      << request_result.ToString();
  }

  std::cout << "Test completed." << std::endl;
  NEARBY_LOGS(INFO) << "Test completed.";
}

}  // namespace nearby::windows
