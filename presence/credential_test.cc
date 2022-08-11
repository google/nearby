// Copyright 2020 Google LLC
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

#include <cstdlib>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/uuid.h"
#include "internal/proto/credential.pb.h"

namespace nearby {
namespace presence {
namespace {
using ::nearby::internal::DeviceMetadata;
using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;
using ::nearby::internal::IdentityType::IDENTITY_TYPE_PROVISIONED;
using ::nearby::internal::IdentityType::IDENTITY_TYPE_PUBLIC;

using ::protobuf_matchers::EqualsProto;

TEST(CredentialsTest, NoDefaultConstructor) {
  EXPECT_FALSE(std::is_trivially_constructible<PrivateCredential>::value);
  EXPECT_FALSE(std::is_trivially_constructible<PublicCredential>::value);
}

TEST(CredentialsTest, InitPublicCredential) {
  PublicCredential pc1 = {};
  PublicCredential pc2 = {};
  EXPECT_THAT(pc1, EqualsProto(pc2));
  pc1.set_identity_type(IDENTITY_TYPE_PUBLIC);
  EXPECT_THAT(pc1, ::testing::Not(EqualsProto(pc2)));
  pc2.set_identity_type(IDENTITY_TYPE_PUBLIC);
  EXPECT_THAT(pc1, EqualsProto(pc2));
}

TEST(CredentialsTest, InitPrivateCredential) {
  PrivateCredential pc1 = {};
  PrivateCredential pc2 = {};
  EXPECT_THAT(pc1, EqualsProto(pc2));
  pc1.set_identity_type(IDENTITY_TYPE_PUBLIC);
  EXPECT_THAT(pc1, ::testing::Not(EqualsProto(pc2)));
  pc2.set_identity_type(IDENTITY_TYPE_PUBLIC);
  EXPECT_THAT(pc1, EqualsProto(pc2));
}

TEST(CredentialsTest, CopyPrivateCredential) {
  PrivateCredential pc1 = {};
  pc1.set_identity_type(IDENTITY_TYPE_PROVISIONED);
  auto salts = pc1.mutable_consumed_salts();
  salts->insert(std::pair<int32, bool>(15, true));
  pc1.mutable_device_metadata()->set_device_name("Android Phone");
  pc1.mutable_device_metadata()->set_device_type(DeviceMetadata::PHONE);
  PrivateCredential pc1_copy = {pc1};
  EXPECT_THAT(pc1, EqualsProto(pc1_copy));
}

TEST(CredentialsTest, CopyPublicCredential) {
  PublicCredential pc1 = {};
  pc1.set_identity_type(IDENTITY_TYPE_PROVISIONED);
  for (const uint8_t byte : location::nearby::Uuid().data()) {
    pc1.mutable_secret_id()->push_back(byte);
  }
  PublicCredential pc1_copy = {pc1};
  EXPECT_THAT(pc1, EqualsProto(pc1_copy));
}
}  // namespace
}  // namespace presence
}  // namespace nearby
