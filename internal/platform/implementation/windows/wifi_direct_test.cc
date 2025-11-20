// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/windows/wifi_direct.h"

#include <iostream>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/logging.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace windows {
namespace {

TEST(WifiDirectMedium, DISABLED_StartWifiDirect) {
  int run_test;
  LOG(INFO) << "Run StartWifiDirect test case? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    winrt::init_apartment();
    WifiDirectCredentials credentials;
    WifiDirectMedium wifi_direct_medium;

    EXPECT_TRUE(wifi_direct_medium.IsInterfaceValid());
    EXPECT_TRUE(wifi_direct_medium.StartWifiDirect(&credentials));

    while (true) {
      LOG(INFO) << "Enter \"s\" to stop test:";
      std::string stop;
      std::cin >> stop;
      if (stop == "s") {
        LOG(INFO) << "Exit WiFi WifiDirect GO";
        EXPECT_TRUE(wifi_direct_medium.StopWifiDirect());
        break;
      }
    }
  } else {
    LOG(INFO) << "Skip the test";
  }
}

TEST(WifiDirectMedium, DISABLED_ConnectWifiDirect) {
  int run_test;
  LOG(INFO) << "Run ConnectWifiDirect GO test case? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    WifiDirectCredentials credentials;
    WifiDirectMedium wifi_direct_medium;

    LOG(INFO) << "Enter WifiDirect Service Name to be connected: ";
    std::string service_name;
    std::cin >> service_name;
    LOG(INFO) << "Enter pin: ";
    std::string pin;
    std::cin >> pin;
    credentials.SetServiceName(service_name);
    credentials.SetPin(pin);

    EXPECT_TRUE(wifi_direct_medium.ConnectWifiDirect(credentials));

    absl::SleepFor(absl::Seconds(2));
    while (true) {
      LOG(INFO) << "Enter \"s\" to stop test:";
      std::string stop;
      std::cin >> stop;
      if (stop == "s") {
        LOG(INFO) << "Disconnect WifiDirect GO";
        EXPECT_TRUE(wifi_direct_medium.DisconnectWifiDirect());
        break;
      }
    }
  } else {
    LOG(INFO) << "Skip the test";
  }
}

TEST(WifiDirectMedium, DISABLED_WifiDirectServerStartListen) {
  int run_test;
  LOG(INFO) << "Run WifiDirectServerStartListen test? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    winrt::init_apartment();
    WifiDirectCredentials credentials;
    WifiDirectMedium wifi_direct_medium;

    EXPECT_TRUE(wifi_direct_medium.IsInterfaceValid());
    EXPECT_TRUE(wifi_direct_medium.StartWifiDirect(&credentials));

    absl::SleepFor(absl::Seconds(1));
    std::unique_ptr<api::WifiDirectServerSocket> server_socket =
        wifi_direct_medium.ListenForService(/*port=*/1234);

    absl::SleepFor(absl::Seconds(60));
    std::unique_ptr<api::WifiDirectSocket> client_socket =
        server_socket->Accept();
    EXPECT_NE(client_socket, nullptr);

    while (true) {
      LOG(INFO) << "Enter \"s\" to stop test:";
      std::string stop;
      std::cin >> stop;
      if (stop == "s") {
        LOG(INFO) << "Close server socket and stop WiFi Direct Service";
        server_socket->Close();
        EXPECT_TRUE(wifi_direct_medium.StopWifiDirect());
        break;
      }
    }
  } else {
    LOG(INFO) << "Skip the test";
  }
}

TEST(WifiDirectMedium, DISABLED_WifiDirectConnectToServiceServer) {
  int run_test;
  LOG(INFO) << "Run WifiDirectConnectToServiceServer test? input 0 or 1:";
  std::cin >> run_test;

  if (run_test) {
    winrt::init_apartment();
    WifiDirectCredentials credentials;
    WifiDirectMedium wifi_direct_medium;

    LOG(INFO) << "Enter WifiDirect Service Name to be connected: ";
    std::string service_name;
    std::cin >> service_name;
    LOG(INFO) << "Enter pin: ";
    std::string pin;
    std::cin >> pin;
    credentials.SetServiceName(service_name);
    credentials.SetPin(pin);

    EXPECT_TRUE(wifi_direct_medium.ConnectWifiDirect(credentials));
    absl::SleepFor(absl::Seconds(1));
    std::unique_ptr<api::WifiDirectSocket> client_socket =
        wifi_direct_medium.ConnectToService(
            /*ip_address=*/"",
            /*port=*/1234,
            /*cancellation_flag=*/nullptr);
    absl::SleepFor(absl::Seconds(10));

    while (true) {
      LOG(INFO) << "Enter \"s\" to stop test:";
      std::string stop;
      std::cin >> stop;
      if (stop == "s") {
        LOG(INFO) << "Close client socket and disconnect WiFi Direct Service";
        client_socket->Close();
        EXPECT_TRUE(wifi_direct_medium.DisconnectWifiDirect());
        break;
      }
    }
  } else {
    LOG(INFO) << "Skip the test";
  }
}

}  // namespace
}  // namespace windows
}  // namespace nearby
