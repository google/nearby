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

#ifndef CORE_STRATEGY_H_
#define CORE_STRATEGY_H_

#include "platform/port/string.h"

namespace location {
namespace nearby {
namespace connections {

struct Strategy {
 public:
  static const Strategy kP2PCluster;
  static const Strategy kP2PStar;
  static const Strategy kP2PPointToPoint;

  Strategy(const Strategy& that);

  bool isValid() const;
  std::string getName() const;

  friend bool operator==(const Strategy& lhs, const Strategy& rhs);
  friend bool operator!=(const Strategy& lhs, const Strategy& rhs);

 private:
  struct ConnectionType {
    enum Value { P2P = 1 };
  };
  struct TopologyType {
    enum Value { ONE_TO_ONE = 1, ONE_TO_N = 2, M_TO_N = 3 };
  };

  const ConnectionType::Value connection_type;
  const TopologyType::Value topology_type;

  Strategy(ConnectionType::Value connection_type,
           TopologyType::Value topology_type);
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_STRATEGY_H_
