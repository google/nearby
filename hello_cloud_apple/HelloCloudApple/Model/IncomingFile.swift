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

@Observable class IncomingFile: File, Identifiable, Hashable, CustomStringConvertible, Encodable, Decodable {
  enum State: Int {
    case received, uploaded, downloading, downloaded
  }

  // This is the Identifiable.id used by SwiftUI, not the file ID used for identifying the file
  // across devices and the cloud
  let id: UUID = UUID()
  let mimeType: String

  @ObservationIgnored var fileId: String = ""
  @ObservationIgnored var fileSize: Int64 = 0
  @ObservationIgnored var remotePath: String? = nil
  @ObservationIgnored var localUrl: URL? = nil
  var state: State = .received

  var description: String {
    String(format: "\(mimeType), %.1f KB", (Double(fileSize)/1024.0))
  }
  
  init(mimeType: String) {
    self.mimeType = mimeType
  }

  func download(completion: ((_: URL?, _: Error?) -> Void)? = nil) -> Void {
    if state != .uploaded {
      print("The file is being downloading or has already been downloaded. Skipping.")
      completion?(nil, NSError(domain:"Downloading", code:1))
      return
    }

    guard let remotePath else {
      print("Remote path not set. Skipping.")
      completion?(nil, NSError(domain:"Downloading", code:1))
      return
    }

    state = .downloading

    let localPath = UUID().uuidString
    CloudStorage.shared.download(remotePath, as: localPath) { [weak self]
      url, error in
      guard let self else { return }
      self.localUrl = url
      self.state = error == nil ? .downloaded : .received
      completion?(url, error)
    }
  }
  
  static func == (lhs: IncomingFile, rhs: IncomingFile) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher){ hasher.combine(self.id) }

  enum CodingKeys: String, CodingKey {
    case fileId, mimeType, fileSize, remotePath
  }
}
