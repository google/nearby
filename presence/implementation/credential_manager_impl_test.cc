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

#include "presence/implementation/credential_manager_impl.h"

#include <string>

#include "net/proto2/contrib/parse_proto/testing.h"
#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/proto/credential.pb.h"
#include "presence/implementation/encryption.h"

namespace nearby {
namespace presence {
using ::location::nearby::Crypto;

using ::nearby::internal::DeviceMetadata;
using ::nearby::internal::PrivateCredential;
using ::nearby::internal::PublicCredential;
using ::nearby::internal::IdentityType::IDENTITY_TYPE_PRIVATE;
using ::proto2::contrib::parse_proto::ParseTestProto;
using ::protobuf_matchers::EqualsProto;

// TODO(b/241926454): Make sure CredentialManager builds with Github.
TEST(CredentialManagerImpl, CreateOneCredentialSuccessfully) {
  DeviceMetadata device_metadata = ParseTestProto(R"pb(
    stable_device_id: "test_device_id"
    ;
    account_name: "test_account";
    device_name: "NP test device";
    icon_url: "test_image.test.com"
    bluetooth_mac_address: "FF:FF:FF:FF:FF:FF";
    device_type: PHONE;
  )pb");

  CredentialManagerImpl credential_manager;
  auto credentials = credential_manager.CreatePrivateCredential(
      device_metadata, IDENTITY_TYPE_PRIVATE, /* start_time_ms= */ 0,
      /* end_time_ms= */ 1000);

  PrivateCredential* private_credential = credentials.first.get();
  PublicCredential* public_credential = credentials.second.get();

  // Verify the private credential.
  EXPECT_THAT(private_credential->device_metadata(),
              EqualsProto(device_metadata));
  EXPECT_EQ(private_credential->identity_type(), IDENTITY_TYPE_PRIVATE);
  EXPECT_FALSE(private_credential->secret_id().empty());
  EXPECT_EQ(private_credential->start_time_millis(), 0);
  EXPECT_EQ(private_credential->end_time_millis(), 1000);
  EXPECT_EQ(private_credential->authenticity_key().size(),
            CredentialManagerImpl::kAuthenticityKeyByteSize);
  EXPECT_FALSE(private_credential->verification_key().empty());
  EXPECT_EQ(private_credential->metadata_encryption_key().size(),
            CredentialManagerImpl::kAuthenticityKeyByteSize);

  // Verify the public credential.
  EXPECT_EQ(public_credential->identity_type(), IDENTITY_TYPE_PRIVATE);
  EXPECT_FALSE(public_credential->secret_id().empty());
  EXPECT_EQ(private_credential->authenticity_key(),
            public_credential->authenticity_key());
  EXPECT_EQ(public_credential->start_time_millis(), 0);
  EXPECT_EQ(public_credential->end_time_millis(), 1000);
  EXPECT_EQ(Crypto::Sha256(private_credential->metadata_encryption_key())
                .AsStringView(),
            public_credential->metadata_encryption_key_tag());
  EXPECT_FALSE(public_credential->verification_key().empty());
  EXPECT_FALSE(public_credential->encrypted_metadata_bytes().empty());

  // Decrypt the device metadata

  auto decrypted_device_metadata = credential_manager.DecryptDeviceMetadata(
      private_credential->metadata_encryption_key(),
      public_credential->authenticity_key(),
      public_credential->encrypted_metadata_bytes());

  EXPECT_EQ(private_credential->device_metadata().SerializeAsString(),
            decrypted_device_metadata);
}
}  // namespace presence
}  // namespace nearby
