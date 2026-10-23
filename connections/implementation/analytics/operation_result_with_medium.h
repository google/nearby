// Copyright 2026 Google LLC
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

#ifndef ANALYTICS_OPERATION_RESULT_WITH_MEDIUM_H_
#define ANALYTICS_OPERATION_RESULT_WITH_MEDIUM_H_

#include <optional>

#include "proto/connections_enums.pb.h"

namespace nearby::analytics {

struct OperationResultWithMedium {
  location::nearby::proto::connections::Medium medium =
      location::nearby::proto::connections::UNKNOWN_MEDIUM;
  std::optional<int> update_index;
  location::nearby::proto::connections::OperationResultCategory
      result_category = location::nearby::proto::connections::CATEGORY_UNKNOWN;
  location::nearby::proto::connections::OperationResultCode result_code =
      location::nearby::proto::connections::DETAIL_UNKNOWN;
  std::optional<location::nearby::proto::connections::ConnectionMode>
      connection_mode;

  void set_medium(location::nearby::proto::connections::Medium m) {
    medium = m;
  }
  void set_update_index(int i) { update_index = i; }
  void set_result_category(
      location::nearby::proto::connections::OperationResultCategory c) {
    result_category = c;
  }
  void set_result_code(
      location::nearby::proto::connections::OperationResultCode c) {
    result_code = c;
  }
  void set_connection_mode(
      location::nearby::proto::connections::ConnectionMode m) {
    connection_mode = m;
  }
};

}  // namespace nearby::analytics

#endif  // ANALYTICS_OPERATION_RESULT_WITH_MEDIUM_H_
