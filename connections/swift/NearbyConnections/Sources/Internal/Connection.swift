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

class InternalConnection: GNCConnectionDelegate {
  weak var delegate: InternalConnectionDelegate?

  func connected(
    toEndpoint endpointID: String,
    withEndpointInfo info: Data,
    authenticationToken: String
  ) {
    delegate?.connected(
      toEndpoint: endpointID,
      withEndpointInfo: info,
      authenticationToken: authenticationToken
    )
  }

  func acceptedConnection(toEndpoint endpointID: String) {
    delegate?.acceptedConnection(toEndpoint: endpointID)
  }

  func rejectedConnection(toEndpoint endpointID: String, with status: GNCStatus) {
    delegate?.rejectedConnection(toEndpoint: endpointID, with: status)
  }

  func disconnected(fromEndpoint endpointID: String) {
    delegate?.disconnected(fromEndpoint: endpointID)
  }
}

protocol InternalConnectionDelegate: AnyObject {
  func connected(
    toEndpoint endpointID: String,
    withEndpointInfo info: Data,
    authenticationToken: String
  )
  func acceptedConnection(toEndpoint endpointID: String)
  func rejectedConnection(toEndpoint endpointID: String, with status: GNCStatus)
  func disconnected(fromEndpoint endpointID: String)
}
