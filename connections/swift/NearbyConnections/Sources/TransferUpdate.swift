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

import Foundation

/// Indicates the current status of the transfer.
public enum TransferUpdate {
  /// The remote endpoint has successfully received the full transfer.
  case success
  /// Either the local or remote endpoint has canceled the transfer.
  case canceled
  /// The remote endpoint failed to receive the transfer.
  case failure
  /// The the transfer is currently in progress with an associated progress value.
  case progress(Progress)
}
