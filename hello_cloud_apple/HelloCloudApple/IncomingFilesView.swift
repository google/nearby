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

import SwiftUI

struct IncomingFilesView: View {
  var model: Endpoint

  let download: () -> Void = {}

  var body: some View {
    // TODO: make the button float at the button; add a vertical scrollbar
    Form {
      Section(header: Text("Incoming Files")) {
            Button(action: download) {
              Label("Download", systemImage: "arrow.down.circle.fill")
            }

        ForEach(model.incomingFiles) {file in
          HStack {
            Label(file.localPath, systemImage: "doc")
            Spacer()
            if (file.isDownloaded) {
              Image(systemName: "circle.fill").foregroundColor(.green)
            } else if (file.isDownloading) {
              ProgressView()
            } else {
              Image(systemName: "circle.fill").foregroundColor(.gray)
            }
          }
        }
      }
      .headerProminence(.increased)
    }
    .navigationTitle(model.name)
  }
}

struct IncomingFilesView__Previews: PreviewProvider {
  static var previews: some View {
    IncomingFilesView(
      model: Endpoint(
        id: "R2D2",
        name: "Debug droid",
        state: Endpoint.State.discovered,
        isIncoming: false,
        incomingFiles: [
          IncomingFile(localPath: "IMG_0001.jpg", remotePath: "1234567890ABCDEF", fileSize: 4000000, isDownloading: true, isDownloaded: false),
          IncomingFile(localPath: "IMG_0002.jpg", remotePath: "1234567890ABCDEF", fileSize: 5000000, isDownloading: false, isDownloaded: false),
          IncomingFile(localPath: "IMG_0003.jpg", remotePath: "1234567890ABCDEF", fileSize: 5000000, isDownloading: false, isDownloaded: true)
        ]
      )
    )
  }
}
