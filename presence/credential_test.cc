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

#include "internal/platform/uuid.h"
#include "third_party/nearby/presence/credential.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "third_party/nearby/presence/presence_identity.h"

namespace nearby {
namespace presence {
namespace {

TEST(CredentialsTest, NoDefaultConstructor) {
  EXPECT_FALSE(std::is_trivially_constructible<PrivateCredential>::value);
  EXPECT_FALSE(std::is_trivially_constructible<PublicCredential>::value);
}

TEST(CredentialsTest, InitPublicCredential) {
  PublicCredential pc1 = {};
  PublicCredential pc2 = {};
  EXPECT_EQ(pc1, pc2);
  pc1.identity_type = PresenceIdentity::IdentityType::kPublic;
  EXPECT_FALSE(pc1 == pc2);
  pc2.identity_type = PresenceIdentity::IdentityType::kPublic;
  EXPECT_EQ(pc1, pc2);
}

TEST(CredentialsTest, InitPrivateCredential) {
  PrivateCredential pc1 = {};
  PrivateCredential pc2 = {};
  EXPECT_EQ(pc1, pc2);
  pc1.identity_type = PresenceIdentity::IdentityType::kPublic;
  EXPECT_FALSE(pc1 == pc2);
  pc2.identity_type = PresenceIdentity::IdentityType::kPublic;
  EXPECT_EQ(pc1, pc2);
}

TEST(CredentialsTest, CopyPrivateCredential) {
  PrivateCredential pc1 = {};
  pc1.identity_type = PresenceIdentity::IdentityType::kProvisioned;
  std::vector<uint8_t> salts = {};
  salts.push_back(15);
  pc1.consumed_salts.insert(salts.begin(), salts.end());
  pc1.device_metadata.set_device_name("Android Phone");
  pc1.device_metadata.set_device_type(proto::DeviceMetadata::PHONE);
  PrivateCredential pc1_copy = {pc1};
  EXPECT_EQ(pc1, pc1_copy);
}

TEST(CredentialsTest, CopyPublicCredential) {
  PublicCredential pc1 = {};
  pc1.identity_type = PresenceIdentity::IdentityType::kProvisioned;
  for (const uint8_t byte : location::nearby::Uuid().data()) {
    pc1.secret_id.emplace_back(byte);
  }
  PublicCredential pc1_copy = {pc1};
  EXPECT_EQ(pc1, pc1_copy);
}
}  // namespace
}  // namespace presence
}  // namespace nearby
