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

#include "connections/implementation/mediums/wifi.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace connections {
namespace {

TEST(WifiTest, ConstructorDestructorWorks) {
  Wifi wifi_a;
  Wifi wifi_b;

  if (wifi_a.IsAvailable() && wifi_b.IsAvailable()) {
    // Make sure we can create 2 distinct mediums.
    EXPECT_NE(&wifi_a, &wifi_b);
  }
  // TODO(b/233324423): Add test coverage for wifi.h
}

TEST(WifiTest, CanGetCapability) {
  // TODO(b/233324423): Add test coverage for wifi.h
}

TEST(WifiTest, CanInformation) {
  // TODO(b/233324423): Add test coverage for wifi.h
}


}  // namespace
}  // namespace connections
}  // namespace nearby
