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
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_C_STRATEGY_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_C_STRATEGY_W_H_

#include <string>

#include "connections/c/dll_config.h"

namespace nearby::windows {

// Defines a copyable, comparable connection strategy type.
// It is one of: kP2pCluster, kP2pStar, kP2pPointToPoint.
class DLL_API StrategyW {
 public:
  static const StrategyW kNone;
  static const StrategyW kP2pCluster;
  static const StrategyW kP2pStar;
  static const StrategyW kP2pPointToPoint;
  constexpr StrategyW() : StrategyW(kNone) {}
  constexpr StrategyW(const StrategyW&) = default;
  constexpr StrategyW& operator=(const StrategyW&) = default;

  // Helper functions for exe to access static member variables in the dll.
  static const StrategyW& GetStrategyNone();
  static const StrategyW& GetStrategyP2pCluster();
  static const StrategyW& GetStrategyP2pStar();
  static const StrategyW& GetStrategyPointToPoint();

  // Returns true, if strategy is kNone, false otherwise.
  bool IsNone() const;

  // Returns true, if a strategy is one of the supported strategies,
  // false otherwise.
  bool IsValid() const;

  // Returns a string representing given strategy, for every valid strategy.
  std::string GetName() const;

  // Undefined strategy.
  void Clear();
  friend bool operator==(const StrategyW& lhs, const StrategyW& rhs);
  friend bool operator!=(const StrategyW& lhs, const StrategyW& rhs);

 private:
  enum class ConnectionType {
    kNone = 0,
    kPointToPoint = 1,
  };
  enum class TopologyType {
    kUnknown = 0,
    kOneToOne = 1,
    kOneToMany = 2,
    kManyToMany = 3,
  };
  constexpr StrategyW(ConnectionType connection_type,
                      TopologyType topology_type)
      : connection_type_(connection_type), topology_type_(topology_type) {}
  ConnectionType connection_type_;
  TopologyType topology_type_;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_STRATEGY_W_H_
