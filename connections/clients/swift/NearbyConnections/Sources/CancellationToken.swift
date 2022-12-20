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

/// Token used to cancel a transfer.
public class CancellationToken {
  internal let payloadID: Int64

  internal init(_ payloadID: Int64) {
    self.payloadID = payloadID
  }

  /// Cancel the ongoing transfer.
  public func cancel() {
    // TODO(b/257824412): Handle errors from completion handler.
    GNCCoreAdapter.shared.cancelPayload(payloadID)
  }
}
