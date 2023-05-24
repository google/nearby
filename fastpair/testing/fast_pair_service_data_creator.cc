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

#include "fastpair/testing/fast_pair_service_data_creator.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/escaping.h"

namespace nearby {
namespace fastpair {

FastPairServiceDataCreator::Builder::Builder() = default;

FastPairServiceDataCreator::Builder::~Builder() = default;

FastPairServiceDataCreator::Builder&
FastPairServiceDataCreator::Builder::SetHeader(uint8_t byte) {
  header_ = byte;
  return *this;
}

FastPairServiceDataCreator::Builder&
FastPairServiceDataCreator::Builder::SetModelId(absl::string_view model_id) {
  model_id_ = std::string(model_id);
  return *this;
}

FastPairServiceDataCreator::Builder&
FastPairServiceDataCreator::Builder::AddExtraFieldHeader(uint8_t header) {
  extra_field_headers_.push_back(header);
  return *this;
}

FastPairServiceDataCreator::Builder&
FastPairServiceDataCreator::Builder::AddExtraField(absl::string_view field) {
  extra_fields_.push_back(std::string(field));
  return *this;
}

std::unique_ptr<FastPairServiceDataCreator>
FastPairServiceDataCreator::Builder::Build() {
  return std::make_unique<FastPairServiceDataCreator>(
      header_, model_id_, extra_field_headers_, extra_fields_);
}

FastPairServiceDataCreator::FastPairServiceDataCreator(
    std::optional<uint8_t> header, std::optional<std::string> model_id,
    std::vector<uint8_t> extra_field_headers,
    std::vector<std::string> extra_fields)
    : header_(header),
      model_id_(model_id),
      extra_field_headers_(extra_field_headers),
      extra_fields_(extra_fields) {}

FastPairServiceDataCreator::~FastPairServiceDataCreator() = default;

std::vector<uint8_t> FastPairServiceDataCreator::CreateServiceData() {
  if (extra_field_headers_.size() != extra_fields_.size()) {
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> service_data;

  if (header_) service_data.push_back(header_.value());

  if (model_id_) {
    std::string model_id_bytes = absl::HexStringToBytes(model_id_.value());
    std::move(std::begin(model_id_bytes), std::end(model_id_bytes),
              std::back_inserter(service_data));
  }

  for (size_t i = 0; i < extra_field_headers_.size(); i++) {
    service_data.push_back(extra_field_headers_[i]);
    std::string extra_field_bytes = absl::HexStringToBytes(extra_fields_[i]);
    std::move(std::begin(extra_field_bytes), std::end(extra_field_bytes),
              std::back_inserter(service_data));
  }

  return service_data;
}

}  // namespace fastpair
}  // namespace nearby
