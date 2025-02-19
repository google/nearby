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

/// Indicates the mediums for advertisement, discovery or connection for the communication
/// with the NearbyConnections caller.
public enum Medium {
  case bluetooth(Bool)
  case ble(Bool)
  case webrtc(Bool)
  case wifilan(Bool)
  case wifihotspot(Bool)
  case wifidirect(Bool)

  /// Returns the index of the medium in the array.
  func getIndex() -> Int {
    switch self {
    case .bluetooth:
      return 0
    case .ble:
      return 1
    case .webrtc:
      return 2
    case .wifilan:
      return 3
    case .wifihotspot:
      return 4
    case .wifidirect:
      return 5
    }
  }

  /// Returns the value of the medium.
  func getValue() -> Bool {
    switch self {
    case .bluetooth(let enabled):
      return enabled
    case .ble(let enabled):
      return enabled
    case .webrtc(let enabled):
      return enabled
    case .wifilan(let enabled):
      return enabled
    case .wifihotspot(let enabled):
      return enabled
    case .wifidirect(let enabled):
      return enabled
    }
  }
}
