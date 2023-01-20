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

class InternalPayload: GNCPayloadDelegate {
  weak var delegate: InternalPayloadDelegate?

  func receivedPayload(_ payload: GNCPayload, fromEndpoint endpointID: String) {
    delegate?.receivedPayload(payload, fromEndpoint: endpointID)
  }

  func receivedProgressUpdate(
    forPayload payloadID: Int64, with status: GNCPayloadStatus, fromEndpoint endpointID: String,
    bytesTransfered: Int64, totalBytes: Int64
  ) {
    delegate?.receivedProgressUpdate(
      forPayload: payloadID, with: status, fromEndpoint: endpointID,
      bytesTransfered: bytesTransfered, totalBytes: totalBytes)
  }
}

protocol InternalPayloadDelegate: AnyObject {
  func receivedPayload(_ payload: GNCPayload, fromEndpoint endpointID: String)
  func receivedProgressUpdate(
    forPayload payloadID: Int64, with status: GNCPayloadStatus, fromEndpoint endpointID: String,
    bytesTransfered: Int64, totalBytes: Int64)
}
