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

#include "presence/implementation/advertisement_decoder_rust_impl.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "np_cpp_ffi_types.h"
#include "nearby_protocol.h"
#include "internal/platform/logging.h"
#include "presence/data_element.h"
#include "presence/implementation/advertisement_decoder.h"

namespace nearby {
namespace presence {
namespace {

absl::StatusOr<nearby_protocol::ActionType> MapAction(const ActionBit action) {
  switch (action) {
    case ActionBit::kActiveUnlockAction:
      return nearby_protocol::ActionType::ActiveUnlock;
    case ActionBit::kNearbyShareAction:
      return nearby_protocol::ActionType::NearbyShare;
    case ActionBit::kInstantTetheringAction:
      return nearby_protocol::ActionType::InstantTethering;
    case ActionBit::kPhoneHubAction:
      return nearby_protocol::ActionType::PhoneHub;
    default:
      return absl::InvalidArgumentError("Unsupported action type");
  }
}

void AddActionsToAdvertisement(const nearby_protocol::V0Actions& parsed_actions,
                               Advertisement& advertisement) {
  for (const auto action : kAllActionBits) {
    auto action_type = MapAction(action);
    if (!action_type.ok()) {
      NEARBY_LOGS(WARNING)
          << "Advertisement contains an unsupported action bit: "
          << (int)action;
      continue;
    }
    if (parsed_actions.HasAction(*action_type)) {
      advertisement.data_elements.push_back(DataElement(action));
    }
  }
}

void ProcessDataElement(const nearby_protocol::V0DataElement& data_element,
                        Advertisement& advertisement) {
  switch (data_element.GetKind()) {
    case nearby_protocol::V0DataElementKind::TxPower: {
      advertisement.data_elements.push_back(DataElement(
          DataElement::kTxPowerFieldType, data_element.AsTxPower().GetAsI8()));
      return;
    }
    case nearby_protocol::V0DataElementKind::Actions: {
      AddActionsToAdvertisement(data_element.AsActions(), advertisement);
    }
  }
}

internal::IdentityType GetIdentityType(
    nearby_protocol::DeserializedV0IdentityKind identity) {
  switch (identity) {
    case np_ffi::internal::DeserializedV0IdentityKind::Plaintext:
      return internal::IdentityType::IDENTITY_TYPE_PUBLIC;
    case np_ffi::internal::DeserializedV0IdentityKind::Decrypted:
      return internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP;
  }
}

absl::StatusOr<::nearby::internal::SharedCredential> FindById(
    std::vector<::nearby::internal::SharedCredential> private_credentials,
    uint64_t id) {
  auto cred =
      std::find_if(private_credentials.begin(), private_credentials.end(),
                   [&id](const auto& x) { return x.id() == id; });
  if (cred == private_credentials.end()) {
    return absl::NotFoundError("No credential found with id: " +
                               std::to_string(id));
  }
  return *cred;
}

absl::Status ProcessLegibleV0Adv(
    nearby_protocol::LegibleDeserializedV0Advertisement legible_adv,
    std::vector<::nearby::internal::SharedCredential> private_credentials,
    Advertisement& advertisement) {
  advertisement.identity_type = GetIdentityType(legible_adv.GetIdentityKind());

  auto num_des = legible_adv.GetNumberOfDataElements();
  auto payload = legible_adv.IntoPayload();

  // TODO(b/333126765): salt isn't a DE, we should restructure the
  // Advertisement struct to reflect this
  if (advertisement.identity_type ==
      internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP) {
    auto cred_details = payload.TryGetIdentityDetails();
    if (!cred_details.ok()) {
      return cred_details.status();
    }

    advertisement.public_credential =
        FindById(private_credentials, cred_details->cred_id);

    // TODO(b/333126765): update salt to use unsigned char * to remove cast
    std::string salt(reinterpret_cast<char const*>(cred_details->salt), 2);
    advertisement.data_elements.push_back(DataElement(0x00, salt));

    std::string metadata_key(
        reinterpret_cast<char const*>(cred_details->identity_token), 14);
    advertisement.metadata_key = std::move(metadata_key);
  }

  for (int i = 0; i < num_des; i++) {
    auto de_result = payload.TryGetDataElement(i);
    if (!de_result.ok()) {
      return de_result.status();
    }
    ProcessDataElement(*de_result, advertisement);
  }
  return absl::OkStatus();
}

absl::Status ProcessV0Advertisement(
    nearby_protocol::DeserializedV0Advertisement result,
    std::vector<::nearby::internal::SharedCredential> private_credentials,
    Advertisement& adv) {
  switch (result.GetKind()) {
    case nearby_protocol::DeserializedV0AdvertisementKind::Legible:
      return ProcessLegibleV0Adv(result.IntoLegible(), private_credentials,
                                 adv);
      break;
    case nearby_protocol::DeserializedV0AdvertisementKind::
        NoMatchingCredentials: {
      return absl::UnavailableError(
          "Couldn't decrypt the message with any credentials");
    }
  }
}

}  // namespace

absl::StatusOr<Advertisement> AdvertisementDecoderImpl::DecodeAdvertisement(
    absl::string_view advertisement) {
  auto byte_buffer = nearby_protocol::ByteBuffer<
      nearby_protocol::MAX_ADV_PAYLOAD_SIZE>::TryFromString(advertisement);
  if (!byte_buffer.ok()) {
    return absl::InvalidArgumentError("Invalid length advertisement");
  }

  Advertisement decoded_advertisement;
  const nearby_protocol::RawAdvertisementPayload payload(byte_buffer.value());
  auto deserialize_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(payload,
                                                              cred_book_);

  switch (deserialize_result.GetKind()) {
    case np_ffi::internal::DeserializeAdvertisementResultKind::Error: {
      return absl::InvalidArgumentError("Invalid advertisement format");
    }
    case np_ffi::internal::DeserializeAdvertisementResultKind::V1: {
      return absl::UnimplementedError(
          absl::StrFormat("V1 Advertisement format is not supported"));
    }
    case np_ffi::internal::DeserializeAdvertisementResultKind::V0: {
      decoded_advertisement.version = 0;
      auto result =
          ProcessV0Advertisement(deserialize_result.IntoV0(),
                                 private_credentials_, decoded_advertisement);
      if (!result.ok()) {
        return result;
      }
      break;
    }
  }

  return decoded_advertisement;
}

nearby_protocol::CredentialBook
AdvertisementDecoderImpl::InitializeCredentialBook(
    absl::flat_hash_map<nearby::internal::IdentityType,
                        std::vector<::nearby::internal::SharedCredential>>*
        credentials_map) {
  if (credentials_map == nullptr) {
    nearby_protocol::CredentialSlab slab;
    nearby_protocol::CredentialBook cred_book(slab);
    return cred_book;
  }

  nearby_protocol::CredentialSlab slab;
  for (const auto& credential : (*credentials_map)
           [internal::IdentityType::IDENTITY_TYPE_PRIVATE_GROUP]) {
    std::vector<uint8_t> metadata_bytes(
        credential.encrypted_metadata_bytes_v0().begin(),
        credential.encrypted_metadata_bytes_v0().end());
    nearby_protocol::MatchedCredentialData matched_cred(credential.id(),
                                                        metadata_bytes);

    auto key_seed = credential.key_seed();
    std::array<uint8_t, 32> key_seed_array;
    std::copy(key_seed.begin(), key_seed.end(), key_seed_array.data());

    auto tag = credential.metadata_encryption_key_tag_v0();
    std::array<uint8_t, 32> tag_array;
    std::copy(tag.begin(), tag.end(), tag_array.data());

    auto matchable_credential = nearby_protocol::V0MatchableCredential(
        key_seed_array, tag_array, matched_cred);
    slab.AddV0Credential(matchable_credential);
  }
  nearby_protocol::CredentialBook cred_book(slab);
  return cred_book;
}

}  // namespace presence
}  // namespace nearby
