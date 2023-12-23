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

  let id: UUID
  let mimeType: String

  @ObservationIgnored var fileSize: Int64 = 0
  @ObservationIgnored var remotePath: String? = nil
  @ObservationIgnored var localUrl: URL? = nil
  var state: State = .received

  var description: String {
    String(format: "\(mimeType), %.1f KB", (Double(fileSize)/1024.0))
  }
  
  init(id: UUID, mimeType: String) {
    self.id = id
    self.mimeType = mimeType
  }

  func download() async -> URL? {
    if state != .uploaded {
      print("The file is being downloading or has already been downloaded. Skipping.")
      return nil
    }

    guard let remotePath else {
      print("Remote path not set. Skipping.")
      return nil
    }

    state = .downloading
    let url = await CloudStorage.shared.download(remotePath, as: UUID().uuidString)
    self.localUrl = url
    state = url != nil ? .downloaded : .uploaded
    return url;
  }
  
  static func == (lhs: IncomingFile, rhs: IncomingFile) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher){ hasher.combine(self.id) }

  enum CodingKeys: String, CodingKey {
    case id, mimeType, fileSize, remotePath
  }
}
