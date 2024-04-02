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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "third_party/beto-core/src/nearby/presence/np_cpp_ffi/include/nearby_protocol.h"
#include "internal/proto/credential.proto.h"
#include "presence/data_element.h"
#include "presence/implementation/advertisement_decoder.h"

namespace nearby {
namespace presence {

AdvertisementDecoderImpl::AdvertisementDecoderImpl(
    absl::flat_hash_map<nearby::internal::IdentityType,
                        std::vector<::nearby::internal::SharedCredential>>*
        credentials_map) {
  // TODO(nwadih): implement this b/330746928
}

absl::StatusOr<Advertisement> AdvertisementDecoderImpl::DecodeAdvertisement(
    absl::string_view advertisement) {
  // TODO(nwadih): implement this b/330746928
  return absl::UnimplementedError("Not implemented");
}

}  // namespace presence
}  // namespace nearby
