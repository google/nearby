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

#include "connections/c/strategy_w.h"

#include <string>

namespace nearby::windows {

const StrategyW StrategyW::kNone = {StrategyW::ConnectionType::kNone,
                                    StrategyW::TopologyType::kUnknown};
const StrategyW StrategyW::kP2pCluster{StrategyW::ConnectionType::kPointToPoint,
                                       StrategyW::TopologyType::kManyToMany};
const StrategyW StrategyW::kP2pStar{StrategyW::ConnectionType::kPointToPoint,
                                    StrategyW::TopologyType::kOneToMany};
const StrategyW StrategyW::kP2pPointToPoint{
    StrategyW::ConnectionType::kPointToPoint,
    StrategyW::TopologyType::kOneToOne};

// static
const StrategyW& StrategyW::GetStrategyNone() { return StrategyW::kNone; }

// static
const StrategyW& StrategyW::GetStrategyP2pCluster() {
  return StrategyW::kP2pCluster;
}

// static
const StrategyW& StrategyW::GetStrategyP2pStar() { return StrategyW::kP2pStar; }

// static
const StrategyW& StrategyW::GetStrategyPointToPoint() {
  return StrategyW::kP2pPointToPoint;
}

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

}  // namespace nearby::windows
