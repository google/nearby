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

#include "internal/platform/blocking_queue_stream.h"

#include "gtest/gtest.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby {
namespace {

TEST(BlockingQueueStreamTest, ReadSuccess) {
  bool is_multiplex_enabled = NearbyFlags::GetInstance().GetBoolFlag(
      connections::config_package_nearby::nearby_connections_feature::
          kEnableMultiplex);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      connections::config_package_nearby::nearby_connections_feature::
          kEnableMultiplex, true);

  BlockingQueueStream stream;
  ByteArray bytes = ByteArray("test1test2test3");
  stream.Write(bytes);
  ExceptionOr<ByteArray> result = stream.Read(5);
  EXPECT_EQ(result.result(), ByteArray("test1"));
  result = stream.Read(5);
  EXPECT_EQ(result.result(), ByteArray("test2"));
  result = stream.Read(5);
  EXPECT_EQ(result.result(), ByteArray("test3"));
  stream.Close();

  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      connections::config_package_nearby::nearby_connections_feature::
          kEnableMultiplex, is_multiplex_enabled);
}

TEST(BlockingQueueStreamTest, MultiplexDisabled) {
  bool is_multiplex_enabled = NearbyFlags::GetInstance().GetBoolFlag(
      connections::config_package_nearby::nearby_connections_feature::
          kEnableMultiplex);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      connections::config_package_nearby::nearby_connections_feature::
          kEnableMultiplex, false);

  BlockingQueueStream stream;
  ByteArray bytes = ByteArray("test1test2test3");
  stream.Write(bytes);
  ExceptionOr<ByteArray> result = stream.Read(5);
  EXPECT_EQ(result, ExceptionOr<ByteArray>(Exception::kExecution));
  stream.Close();

  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      connections::config_package_nearby::nearby_connections_feature::
          kEnableMultiplex, is_multiplex_enabled);
}

}  // namespace
}  // namespace nearby
