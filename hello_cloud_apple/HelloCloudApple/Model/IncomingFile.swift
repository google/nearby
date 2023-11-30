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
  
  let localPath: String
  @ObservationIgnored let remotePath: String
  @ObservationIgnored let fileSize: Int64

  var state: State = .received

  init(localPath: String, remotePath: String, fileSize: Int64, state: State) {
    self.localPath = localPath
    self.remotePath = remotePath
    self.fileSize = fileSize
  }

  func download () -> Void {
    // TODO: async this and actually download
    state = .downloaded
  }
  
  static func == (lhs: IncomingFile, rhs: IncomingFile) -> Bool { lhs.id == rhs.id }
  func hash(into hasher: inout Hasher){ hasher.combine(self.id) }

  enum CodingKeys: String, CodingKey {
    case localPath, remotePath, fileSize
  }

  static func decodeIncomingFiles(fromJson json: Data) -> [IncomingFile]? {
    let decoder = JSONDecoder()
    let result = try? decoder.decode([IncomingFile]?.self, from: json) ?? nil
    return result
  }
}
