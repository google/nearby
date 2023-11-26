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
import SwiftUI

@Observable class Endpoint: Identifiable, Hashable {
  enum State: Int, CustomStringConvertible {
    case discovered, pending, connected, sending, receiving

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

//  enum Medium: Int {
//    case unknown, mDns, bluetooth, wifiHotspot, ble, wifiLan, wifiAware, nfc, wifiDirect, webRtc, bleL2Cap, usb
//  }

  let id: String
  let name: String
  let isIncoming: Bool

  // var medium: Medium
  var state: State
  var outgoingFiles: [OutgoingFile] = []
  var incomingFiles: [IncomingFile] = []
  var transfers: [Transfer] = []

  static func == (lhs: Endpoint, rhs: Endpoint) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher){ hasher.combine(id) }

  init(id: String, name: String, isIncoming: Bool = false, state: State = State.discovered, outgoingFiles: [OutgoingFile] = [], incomingFiles: [IncomingFile] = [], transfers: [Transfer] = []) {
    self.id = id
    self.name = name
    self.isIncoming = isIncoming
    self.state = state
    self.outgoingFiles = outgoingFiles
    self.incomingFiles = incomingFiles
    self.transfers = transfers
  }
}
