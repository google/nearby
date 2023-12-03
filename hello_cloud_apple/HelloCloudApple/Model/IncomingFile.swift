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

@Observable class IncomingFile: Identifiable, Hashable, Decodable {
  enum State: Int {
    case received, downloading, downloaded
  }

  let id: UUID = UUID()
  
  let mimeType: String
  // Suggested file name set by the sender. We don't need to honor it.
  let fileName: String
  // Actual url of the local file, once it's downloaded
  @ObservationIgnored var localUrl: URL?

  @ObservationIgnored let remotePath: String
  @ObservationIgnored let fileSize: Int64

  var state: State = .received

  init(mimeType: String, fileName: String, localUrl: URL? = nil, remotePath: String, fileSize: Int64, state: State = .received) {
    self.mimeType = mimeType
    self.fileName = fileName
    self.localUrl = localUrl
    self.remotePath = remotePath
    self.fileSize = fileSize
    self.state = state
  }

  func download () -> Void {
    if state != .received {
      print("The file is being downloading or has already been downloaded. Skipping.")
      return
    }

    state = .downloading
    let index = remotePath.lastIndex(of: ".")
    let ext = index == nil
      ? ""
      :String(remotePath[index!...])

    let localPath = UUID().uuidString + ext
    CloudStorage.shared.download(remotePath, as: localPath) { [weak self]
      url, error in
      guard let self else { return }
      // TODO: calculate transfer speed and put into the info section
      // let size = (try? url?.resourceValues(forKeys:[.fileSizeKey]).fileSize) ?? 0
      self.localUrl = url
      self.state = error == nil ? .downloaded : .received
    }
  }
  
  static func == (lhs: IncomingFile, rhs: IncomingFile) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher){ hasher.combine(self.id) }

  enum CodingKeys: String, CodingKey {
    case mimeType, fileName, remotePath, fileSize
  }

  static func decodeIncomingFiles(fromJson json: Data) -> [IncomingFile]? {
    let decoder = JSONDecoder()
    let result = try? decoder.decode([IncomingFile]?.self, from: json) ?? nil
    return result
  }
}
