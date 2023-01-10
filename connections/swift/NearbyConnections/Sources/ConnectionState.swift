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

/// Indicates the current status of the connection between a local and remote endpoint.
public enum ConnectionState {
  /// A connection to the remote endpoint is currently being established.
  case connecting
  /// The local endpoint is currently connected to the remote endpoint.
  case connected
  /// The remote endpoint is no longer connected.
  case disconnected
  /// The local or remote endpoint has rejected the connection request.
  case rejected
}
