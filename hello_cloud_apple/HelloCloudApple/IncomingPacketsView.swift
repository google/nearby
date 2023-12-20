//
//  Downloads.swift
//  HelloCloudApple
//
//  Created by Deling Ren on 12/15/23.
//

import SwiftUI

struct IncomingPacketsView: View {
  @EnvironmentObject var model: Main
  @State var imageUrl: URL? = nil

  var body: some View {
    Form {
      Section{
        List {
          ForEach(Array(model.incomingPackets.enumerated()), id: \.1.id) { i, packet in
            DisclosureGroup (isExpanded: $model.incomingPackets[i].expanded) {
              ForEach(packet.files) { file in
                HStack {
                  if file.state == .downloaded {
                    Button(action: {
                      self.imageUrl = file.localUrl
                    }) {
                      Label(String(describing: file.description), systemImage: "photo")
                    }.buttonStyle(.borderless)
                  } else {
                    Image(systemName: "photo")
                    Text(String(describing: file.description))
                  }

                  Spacer()
                  // TODO: turn into a ZStack to keep size fixed
                  switch file.state {
                  case .received:
                    Image(systemName: "circle.dotted").foregroundColor(.gray)
                  case .downloading:
                    ProgressView()
                  case .downloaded:
                    Image(systemName: "circle.fill").foregroundColor(.green)
                  }
                }
              }
            } label: {
              Button(action: { packet.download() }) {
                Image(systemName: "icloud.and.arrow.down.fill")
              }.buttonStyle(.borderless)
              Text(String(describing: packet))
              Spacer()
              // TODO: overlay the download button with the indicator
              // Logic is a little convoluted. Available states:
              // .received: grey dotted circle
              // .uploaded: download button
              // .downloading: spinner
              // .downloaded: green filled circle
              ZStack{
                Group {
                  switch packet.state {
                  case .received:
                    Image(systemName: "circle.dotted").foregroundColor(.gray)
                  case .uploaded:
                    Image(systemName: "circle.fill").foregroundColor(.gray)
                  case .downloaded:
                    Image(systemName: "circle.fill").foregroundColor(.green)
                  default:
                    Image(systemName: "circle.dotted").foregroundColor(.gray)
                  }
                }
                .opacity(packet.state == .downloading ? 0 : 1)
                ProgressView().opacity(packet.state == .downloading ? 1 : 0)
              }
            }
          }
        }
      }
    }
    .navigationTitle("Incoming packets")
  }}

#Preview {
  IncomingPacketsView().environment(Main.shared)
}
