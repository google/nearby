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

#include <string>

namespace location {
namespace nearby {
namespace connections {

// Defines a copyable, comparable connection strategy type.
// It is one of: kP2pCluster, kP2pStar, kP2pPointToPoint.
class Strategy {
 public:
  static const Strategy kNone;
  static const Strategy kP2pCluster;
  static const Strategy kP2pStar;
  static const Strategy kP2pPointToPoint;
  constexpr Strategy() : Strategy(kNone) {}
  constexpr Strategy(const Strategy&) = default;
  constexpr Strategy& operator=(const Strategy&) = default;

  // Returns true, if strategy is kNone, false otherwise.
  bool IsNone() const;

  // Returns true, if a strategy is one of the supported strategies,
  // false otherwise.
  bool IsValid() const;

  // Returns a string representing given strategy, for every valid strategy.
  std::string GetName() const;

  // Undefine strategy.
  void Clear() { *this = kNone; }
  friend bool operator==(const Strategy& lhs, const Strategy& rhs);
  friend bool operator!=(const Strategy& lhs, const Strategy& rhs);

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
  constexpr Strategy(ConnectionType connection_type, TopologyType topology_type)
      : connection_type_(connection_type), topology_type_(topology_type) {}
  ConnectionType connection_type_;
  TopologyType topology_type_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location
#endif  // CORE_STRATEGY_H_
