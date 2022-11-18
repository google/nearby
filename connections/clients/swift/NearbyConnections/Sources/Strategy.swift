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
import NearbyCoreAdapter

/// Indicates the connection strategy and restrictions for advertisement or discovery.
///
/// - Note: Only a subset of mediums are supported by certain strategies as depicted in the table
///   below. Generally, more restrictive strategies have better medium support.
///
/// |              | BT Classic | BLE | LAN | Aware | NFC | USB | Hotspot | Direct | 5GHz WiFi |
/// |--------------|:----------:|:---:|:---:|:-----:|:---:|:---:|:-------:|:------:|:---------:|
/// | cluster      | ✓          | ✓   | ✓   | ✓     | ✓   | ✓   | ×       | ×      | ×         |
/// | star         | ✓          | ✓   | ✓   | ✓     | ✓   | ✓   | ✓       | ✓      | ×         |
/// | pointToPoint | ✓          | ✓   | ✓   | ✓     | ✓   | ✓   | ✓       | ✓      | ✓         |
///
/// - Important: In order to discover another endpoint, the remote endpoint must be advertising
///   using the same strategy.
public enum Strategy {
  /// No restrictions on the amount of incoming and outgoing connections.
  case cluster
  /// Restricts the discoverer to a single connection, while advertisers have no restrictions on
  /// amount of connections.
  case star
  /// Restricts both adverisers and discoverers to a single connection.
  case pointToPoint

  internal var objc: NearbyCoreAdapter.Strategy {
    switch self {
    case .cluster: return .cluster
    case .star: return .star
    case .pointToPoint: return .pointToPoint
    }
  }
}
