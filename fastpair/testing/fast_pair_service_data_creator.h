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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_TESTING_FAST_PAIR_SERVICE_DATA_CREATOR_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_TESTING_FAST_PAIR_SERVICE_DATA_CREATOR_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nearby {
namespace fastpair {

// Convenience class with Builder to create byte arrays which represent Fast
// Pair Service Data.
class FastPairServiceDataCreator {
 public:
  class Builder {
   public:
    Builder();
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    ~Builder();

    Builder& SetHeader(uint8_t byte);
    Builder& SetModelId(std::string_view model_id);
    Builder& AddExtraFieldHeader(uint8_t header);
    Builder& AddExtraField(std::string_view field);
    std::unique_ptr<FastPairServiceDataCreator> Build();

   private:
    std::optional<uint8_t> header_;
    std::optional<std::string> model_id_ = std::nullopt;
    std::vector<uint8_t> extra_field_headers_;
    std::vector<std::string> extra_fields_;
  };

  FastPairServiceDataCreator(std::optional<uint8_t> header,
                             std::optional<std::string> model_id,
                             std::vector<uint8_t> extra_field_headers,
                             std::vector<std::string> extra_fields);
  FastPairServiceDataCreator(const FastPairServiceDataCreator&) = delete;
  FastPairServiceDataCreator& operator=(const FastPairServiceDataCreator&) =
      delete;
  ~FastPairServiceDataCreator();

  std::vector<uint8_t> CreateServiceData();

 private:
  std::optional<uint8_t> header_;
  std::optional<std::string> model_id_;
  std::vector<uint8_t> extra_field_headers_;
  std::vector<std::string> extra_fields_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_TESTING_FAST_PAIR_SERVICE_DATA_CREATOR_H_
