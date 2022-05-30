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

#include <string>

#include "connections/clients/windows/strategy_w.h"

namespace location::nearby::windows {

const StrategyW StrategyW::kNone = {StrategyW::ConnectionType::kNone,
                                    StrategyW::TopologyType::kUnknown};
const StrategyW StrategyW::kP2pCluster{StrategyW::ConnectionType::kPointToPoint,
                                       StrategyW::TopologyType::kManyToMany};
const StrategyW StrategyW::kP2pStar{StrategyW::ConnectionType::kPointToPoint,
                                    StrategyW::TopologyType::kOneToMany};
const StrategyW StrategyW::kP2pPointToPoint{
    StrategyW::ConnectionType::kPointToPoint,
    StrategyW::TopologyType::kOneToOne};

constexpr StrategyW::StrategyW() : StrategyW(kNone) {}

bool StrategyW::IsNone() const { return *this == kNone; }

bool StrategyW::IsValid() const {
  return *this == kP2pStar || *this == kP2pCluster || *this == kP2pPointToPoint;
}

std::string StrategyW::GetName() const {
  if (*this == StrategyW::kP2pCluster) {
    return "P2P_CLUSTER";
  }
  if (*this == StrategyW::kP2pStar) {
    return "P2P_STAR";
  }
  if (*this == StrategyW::kP2pPointToPoint) {
    return "P2P_POINT_TO_POINT";
  }
  return "UNKNOWN";
}

void StrategyW::Clear() { *this = kNone; }

bool operator==(const StrategyW& lhs, const StrategyW& rhs) {
  return lhs.connection_type_ == rhs.connection_type_ &&
         lhs.topology_type_ == rhs.topology_type_;
}

bool operator!=(const StrategyW& lhs, const StrategyW& rhs) {
  return !(lhs == rhs);
}

}  // namespace location::nearby::windows
