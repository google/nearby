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

struct TransfersView: View {
  let model: Endpoint

  @EnvironmentObject var mainModel: Main
  @Environment(\.dismiss) var dismiss

  var body: some View {
    Form {
      Section {
        ForEach(model.transfers) {transfer in
          HStack {
            Label(transfer.localPath, systemImage: String(describing: transfer.direction))
            Spacer()
            let (name, color) = transfer.result.IconNameAndColor;
            Image(systemName: name).foregroundColor(color)
          }
        }
      }
    }
    .navigationTitle("Transfers")
    .onChange(of: mainModel.endpoints) { _, endpoints in
      if !endpoints.contains(model) { dismiss() }
    }
  }
}

extension Transfer.Direction: CustomStringConvertible {
  var description: String {
    switch self {
    case .download: "arrow.down.circle.fill"
    case .upload: "arrow.up.circle.fill"
    case .send: "arrow.up.backward.circle.fill"
    case .receive: "arrow.down.right.circle.fill"
    }
  }
}

extension Transfer.Result {
  var IconNameAndColor: (name: String, color: Color) {
    switch self {
    case .success: (name: "checkmark.circle.fill", color: .green)
    case .failure: (name: "xmark.circle.fill", color: .red)
    case .cancaled: (name: "exclamationmark.circle.fill", color: .orange)
    }
  }
}

#Preview {
  TransfersView(
    model: Endpoint(
      id: "R2D2",
      name: "Debug droid",
      isIncoming: false, state: .discovered,
      transfers: [
        Transfer(
          direction: Transfer.Direction.upload, 
          localPath: "IMG_0001.jpg",
          remotePath: "1234567890ABCDEF",
          result: .success
        ),
        Transfer(
          direction: Transfer.Direction.download, 
          localPath: "IMG_0002.jpg",
          remotePath: "XYZ",
          result: .failure
        ),
        Transfer(
          direction: Transfer.Direction.send,
          localPath: "IMG_0003.jpg",
          remotePath: "XYZ",
          result: .success
        ),
        Transfer(
          direction: Transfer.Direction.receive,
          localPath: "IMG_0004.jpg",
          remotePath: "XYZ",
          result: .cancaled
        )
      ]
    )
  )
}
