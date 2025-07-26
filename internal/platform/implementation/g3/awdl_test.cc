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

#include "internal/platform/implementation/g3/awdl.h"

#include <memory>

#include "gtest/gtest.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/awdl.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/nsd_service_info.h"
#include "thread/fiber/fiber.h"

namespace nearby {
namespace g3 {
namespace {

constexpr char kServiceName[] = "service_name";
constexpr char kServiceType[] = "_Awdl_test._tcp.local";

TEST(AwdlMedium, CanStartStopAdvertising) {
  MediumEnvironment::Instance().Start();
  AwdlMedium medium;
  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(kServiceName);
  nsd_service_info.SetServiceType(kServiceType);

  EXPECT_TRUE(medium.StartAdvertising(nsd_service_info));
  EXPECT_FALSE(medium.StartAdvertising(nsd_service_info));
  EXPECT_TRUE(medium.StopAdvertising(nsd_service_info));
  EXPECT_FALSE(medium.StopAdvertising(nsd_service_info));
  MediumEnvironment::Instance().Stop();
}

TEST(AwdlMedium, CanStartStopDiscovery) {
  MediumEnvironment::Instance().Start();
  AwdlMedium medium;
  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(kServiceName);
  nsd_service_info.SetServiceType(kServiceType);

  EXPECT_TRUE(medium.StartDiscovery(
      nsd_service_info.GetServiceType(),
      {.service_discovered_cb = [&](const NsdServiceInfo& service_info) {}}));
  EXPECT_FALSE(medium.StartDiscovery(
      nsd_service_info.GetServiceType(),
      {.service_discovered_cb = [&](const NsdServiceInfo& service_info) {}}));
  EXPECT_TRUE(medium.StopDiscovery(nsd_service_info.GetServiceType()));
  EXPECT_FALSE(medium.StopDiscovery(nsd_service_info.GetServiceType()));
  MediumEnvironment::Instance().Stop();
}

TEST(AwdlMedium, CanConnect) {
  MediumEnvironment::Instance().Start();
  AwdlMedium medium_a;
  AwdlMedium medium_b;
  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(kServiceName);
  nsd_service_info.SetServiceType(kServiceType);

  std::unique_ptr<api::AwdlServerSocket> server_socket =
      medium_a.ListenForService(0);
  ASSERT_TRUE(server_socket);
  nsd_service_info.SetIPAddress(server_socket->GetIPAddress());
  nsd_service_info.SetPort(server_socket->GetPort());
  medium_a.StartAdvertising(nsd_service_info);
  MediumEnvironment::Instance().Sync();  // Ensure advertising is processed

  std::unique_ptr<api::AwdlSocket> accepted_socket;
  absl::Notification accept_notification;
  thread::Fiber accept_fiber([&]() {
    accepted_socket = server_socket->Accept();
    accept_notification.Notify();
  });

  CancellationFlag cancellation_flag;
  std::unique_ptr<api::AwdlSocket> client_socket =
      medium_b.ConnectToService(nsd_service_info, &cancellation_flag);

  EXPECT_TRUE(
      accept_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  accept_fiber.Join();

  EXPECT_TRUE(client_socket);
  EXPECT_TRUE(accepted_socket);

  if (client_socket) client_socket->Close();
  if (accepted_socket) accepted_socket->Close();
  server_socket->Close();
  MediumEnvironment::Instance().Stop();
}

}  // namespace
}  // namespace g3
}  // namespace nearby
