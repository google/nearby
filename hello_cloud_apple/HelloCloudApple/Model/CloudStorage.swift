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
import FirebaseCore
import FirebaseStorage

class CloudStorage {
  static let shared = CloudStorage()

  let storage: Storage;
  let storageRef: StorageReference;

  init() {
    storage = Storage.storage()
    storageRef = storage.reference()
  }

  func upload(_ data: Data, as remotePath: String, completion: ((_: Error?) -> Void)? = nil) {
    let fileRef = storageRef.child(remotePath)
    let uploadTask = fileRef.putData(data, metadata: nil) { (metadata, error) in
      if error == nil {
        print("Succeed uploading file " + remotePath)
      } else {
        print("Failed uploading file " + remotePath +
              ". Error: " + (error?.localizedDescription ?? ""))
      }
      let size = metadata?.size
      completion?(error)
    }
  }
}
