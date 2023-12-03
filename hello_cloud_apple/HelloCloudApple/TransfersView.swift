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
            Label(String(describing: transfer), systemImage: String(describing: transfer.direction))
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

extension Transfer: CustomStringConvertible {
  var description: String {
    switch self.direction {
    case .upload, .download:
      guard let size, let duration else {
        return "Size & Speed N/A"
      }
      let sizeInMB = Double(size) / 1048576
      return String(format: "%.1f MB at %.1f MB/s", sizeInMB, sizeInMB / duration)
    case .receive, .send: return self.from ?? self.to ?? "To & From Unknown"
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
  let mainModel = Main.createDebugModel()
  return TransfersView(
    model: mainModel.endpoints[1]
  ).environment(mainModel)
}
