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

@Observable class OutgoingFile: File, Hashable, Encodable, Decodable {
  enum State: Int {
    case picked, loading, loaded, uploading, uploaded
  }

  let id: UUID = UUID()

  let mimeType: String
  @ObservationIgnored var fileSize: Int64
  @ObservationIgnored var remotePath: String?

  @ObservationIgnored var localUrl: URL?
  var state: State = .picked

  init(mimeType: String, fileSize: Int64 = 0, remotePath: String? = nil) {
    self.mimeType = mimeType
    self.fileSize = fileSize
    self.remotePath = remotePath
  }

  static func createDebugModel(
    mimeType: String, fileSize: Int64, remotePath: String? = nil, state: State = .picked) -> OutgoingFile {
      let result = OutgoingFile(mimeType: mimeType, fileSize: fileSize, remotePath: remotePath)
      result.state = state
      return result
    }

  func upload(completion: ((_: Int, _: Error?) -> Void)? = nil) -> Void {
    guard let localUrl else {
      print("Media not saved. Skipping uploading.")
      return;
    }
    if state != .loaded {
      print("Media not saved. Skipping uploading.")
      return
    }

    remotePath = UUID().uuidString
    if mimeType == "image/jpeg" {
      remotePath! += ".jpeg"
    } else if mimeType == "image/png" {
      remotePath! += ".png"
    }

    state = .uploading

    CloudStorage.shared.upload(from: localUrl, to: remotePath!) { [weak self]
      size, error in
      self?.state = error == nil ? .uploaded : .loaded
      completion?(size, error)
    }
  }

  static func == (lhs: OutgoingFile, rhs: OutgoingFile) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher) { hasher.combine(self.id) }

  enum CodingKeys: String, CodingKey {
    case mimeType, remotePath, fileSize
  }

  static func encodeOutgoingFiles(_ files: [OutgoingFile]) -> Data? {
    return try? JSONEncoder().encode(files)
  }
}
