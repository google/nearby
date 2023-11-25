//
//  Copyright 2023 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

import Foundation
import NearbyConnections

struct Endpoint: Identifiable, Hashable {
  enum State: Int {
    case discovered, pending, connected, sending, receiving
  }

  enum Medium: Int {
    case unknown, mDns, bluetooth, wifiHotspot, ble, wifiLan, wifiAware, nfc, wifiDirect, webRtc, bleL2Cap, usb
  }

  let id: EndpointID
  let name: String
  // let medium: Medium
  let state: State
  let isIncoming: Bool

  var outgoingFiles: [OutgoingFile] = []
  var incomingFiles: [IncomingFile] = []
  var transfers: [Transfer] = []

  static func == (lhs: Endpoint, rhs: Endpoint) -> Bool {
    return lhs.id == rhs.id
  }
}

extension Endpoint.State: CustomStringConvertible {
  var description: String {
    return switch self {
    case .discovered: "Discovered"
    case .pending: "Pending"
    case .connected: "Connected"
    case .sending: "Sending"
    case .receiving: "Receiving"
    }
  }
}
