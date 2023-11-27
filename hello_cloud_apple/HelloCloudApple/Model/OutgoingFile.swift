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

@Observable class OutgoingFile: Identifiable, Hashable {
  enum State: Int {
    case picked, loading, loaded, uploading, uploaded
  }

  let id: UUID = UUID()
  
  let localPath: String
  let fileSize: UInt64

  var state: State
  var remotePath: String?

  init(localPath: String, fileSize: UInt64, state: State = .picked, remotePath: String? = nil) {
    self.localPath = localPath
    self.fileSize = fileSize
    self.state = state
    self.remotePath = remotePath
  }

  func upload() -> Void {
    // TODO: async this and actually upload
    state = .uploaded
    remotePath = "1234567890ABCDEF"
  }

  static func == (lhs: OutgoingFile, rhs: OutgoingFile) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher) { hasher.combine(self.id) }
}
