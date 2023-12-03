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

struct Transfer: Identifiable, Hashable {
  internal init(direction: Transfer.Direction, remotePath: String, result: Transfer.Result, 
                size: Int? = nil, duration: TimeInterval? = nil,
                from: String? = nil, to: String? = nil) {
    self.direction = direction
    self.remotePath = remotePath
    self.result = result
    self.size = size
    self.duration = duration
    self.from = from
    self.to = to
  }
  
  enum Direction {
    case send, receive, upload, download
  }

  enum Result {
    case success, failure, cancaled
  }

  let id: UUID = UUID()

  let direction: Direction
  let remotePath: String
  let result: Result

  let size: Int?
  let duration: TimeInterval?
  let from: String?
  let to: String?
}
