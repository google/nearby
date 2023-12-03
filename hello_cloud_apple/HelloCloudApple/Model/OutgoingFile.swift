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

@Observable class OutgoingFile: Identifiable, Hashable, Encodable {
  enum State: Int {
    case picked, loading, loaded, uploading, uploaded
  }

  let id: UUID = UUID()
  
  // A suggested name for the receiver. It does not serve any other purposes. On iOS, we are only
  // picking images which don't have names. So we'll just use a UUID. On Windows, we use
  // the local file name.
  let fileName: String
  let mimeType: String
  var state: State

  @ObservationIgnored var fileSize: UInt64
  @ObservationIgnored var data: Data?
  @ObservationIgnored var remotePath: String?

  init(mimeType: String, fileSize: UInt64 = 0, state: State = .picked, remotePath: String? = nil) {
    self.mimeType = mimeType
    self.fileSize = fileSize
    self.state = state
    self.remotePath = remotePath

    if mimeType == "image/jpeg" {
      fileName = UUID().uuidString + ".jpeg"
    } else if mimeType == "image/png" {
      fileName = UUID().uuidString + ".png"
    } else {
      fileName = UUID().uuidString
    }
  }

  func upload(completion: ((_: Int, _: Error?) -> Void)? = nil) -> Void {
    guard let data else {
      print("Data is not loaded. Skipping uploading.")
      return;
    }
    if state != .loaded {
      print("File is not loaded. Skipping uploading.")
      return
    }
    if fileName.isEmpty {
      print("Local path is empty. This shouldn't happen!!!")
      return
    }
    
    remotePath = UUID().uuidString
    if mimeType == "image/jpeg" {
      remotePath! += ".jpeg"
    } else if mimeType == "image/png" {
      remotePath! += ".png"
    }

    state = .uploading

    CloudStorage.shared.upload(data, as: remotePath!) { [weak self] 
      size, error in
      self?.state = error == nil ? .uploaded : .loaded
      completion?(size, error)
    }
  }

  static func == (lhs: OutgoingFile, rhs: OutgoingFile) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher) { hasher.combine(self.id) }

  enum CodingKeys: String, CodingKey {
    case mimeType, fileName, remotePath, fileSize
  }

  static func encodeOutgoingFiles(_ files: [OutgoingFile]) -> Data? {
    return try? JSONEncoder().encode(files)
  }
}
