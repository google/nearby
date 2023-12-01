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
  let model: Endpoint
  
  @EnvironmentObject var mainModel: Main
  @Environment(\.dismiss) var dismiss
  
  init(model: Endpoint) {
    self.model = model
  }
  
  func download() -> Void  {
    for file in model.incomingFiles {
      if file.state == .received {
        file.download()
        model.transfers.append(Transfer(direction: .download, localPath: file.localPath, remotePath: file.remotePath, result: .success))
      }
    }
  }
  
  var body: some View {
    // TODO: make the button float at the button; add a vertical scrollbar
    Form {
      Section {
        Button(action: download) {
          Label("Download", systemImage: "arrow.down.circle.fill")
        }
        .buttonStyle(.bordered)
        .disabled(model.incomingFiles.isEmpty ||
                  model.incomingFiles.allSatisfy(
                    {$0.state == .downloaded || $0.state == .downloaded}))

        ForEach(model.incomingFiles) {file in
          HStack {
            Label(file.localPath, systemImage: "doc")
            Spacer()
            switch file.state {
            case .downloaded:
              Image(systemName: "circle.fill").foregroundColor(.green)
            case .downloading:
              ProgressView()
            case .received:
              Image(systemName: "circle.fill").foregroundColor(.gray)
            }
          }
        }
      }
    }
    .navigationTitle("Incoming files")
    .onChange(of: mainModel.endpoints) { _, endpoints in
      if !endpoints.contains(model) { dismiss() }
    }
  }
}

#Preview {
  IncomingFilesView(
    model: Endpoint(
      id: "R2D2",
      name: "Debug droid",
      isIncoming: false, state: .discovered,
      incomingFiles: [
        IncomingFile(localPath: "IMG_0001.jpg", remotePath: "1234567890ABCDEF", 
                     fileSize: 4000000, state: .downloading),
        IncomingFile(localPath: "IMG_0002.jpg", remotePath: "1234567890ABCDEF",
                     fileSize: 5000000, state: .received),
        IncomingFile(localPath: "IMG_0003.jpg", remotePath: "1234567890ABCDEF",
                     fileSize: 5000000, state: .downloaded)
      ]
    )
  ).environment(Main.createDebugModel())
}
