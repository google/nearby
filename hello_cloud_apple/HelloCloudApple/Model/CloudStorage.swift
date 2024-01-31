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
    if Config.localCloud {
      storage.useEmulator(withHost: Config.localCloudHost, port: 9199)
    }
    storageRef = storage.reference()

    // Set a short timeout for debugging. The default is 600s
    storage.maxUploadRetryTime = 3
    storage.maxDownloadRetryTime = 3
  }

  func upload(from localUri: URL, to remotePath: String) async -> Int64? {
    let fileRef = storageRef.child(remotePath)
    return await withCheckedContinuation { continuation in
      _ = fileRef.putFile(from: localUri) { metadata, error in
        guard let metadata else {
          print("E: Failed to upload file to \(remotePath). Error: "
                + (error?.localizedDescription ?? ""))
          continuation.resume(returning: nil)
          return
        }

        print("I: Uploaded file " + remotePath)
        continuation.resume(returning: metadata.size as Int64?)
      }
    }
  }

  func download(_ remotePath: String, as fileName: String) async -> URL? {
    let fileRef = storageRef.child(remotePath)

    guard let directoryUrl = try? FileManager.default.url(
      for: .documentDirectory,
      in: .userDomainMask,
      appropriateFor: nil,
      create: true) else {
      print ("E: Failed to obtain directory for downloading.")
      return nil
    }

    let fileUrl = directoryUrl.appendingPathComponent(fileName)
    return await withCheckedContinuation { continuation in
      _ = fileRef.write(toFile: fileUrl) { url, error in
        guard let url else {
          print("E: Failed to download file \(remotePath). Error: "
                + (error?.localizedDescription ?? ""))
          continuation.resume(returning: nil)
          return
        }

        print("I: Downloaded file \(remotePath).")
        continuation.resume(returning: url as URL?)
      }
    }
  }
}
