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

struct OutgoingPacketsView: View {
  @EnvironmentObject var model: Main
  @State var expanded: [Bool] = []

  var body: some View {
    Form {
      Section{
        List {
          ForEach($model.outgoingPackets) { $packet in
            DisclosureGroup (isExpanded: $packet.expanded) {
              ForEach(Array(packet.files)) { file in
                HStack {
                  Image(systemName: "photo")
                  Text(String(describing: file.description))
                  Spacer()
                  switch file.state {
                  case .picked:
                    Image(systemName: "circle.dotted").foregroundColor(.gray)
                  case .loading:
                    ProgressView()
                  case .loaded:
                    Image(systemName: "circle.fill").foregroundColor(.gray)
                  case .uploading:
                    ProgressView()
                  case .uploaded:
                    Image(systemName: "circle.fill").foregroundColor(.green)
                  }
                }
              }
            } label: {
              Button(action: { packet.upload() }) {
                Image(systemName: "icloud.and.arrow.up.fill")
              }.buttonStyle(.borderless)
              Text(String(describing: packet))
              Spacer()
              ProgressView().opacity(packet.state == .uploading ? 1 : 0)
            }
          }
        }
      }
    }
    .navigationTitle("Outgoing packets")
  }
}

#Preview {
  OutgoingPacketsView().environment(Main.createDebugModel())
}
