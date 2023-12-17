//
//  UploadsView.swift
//  HelloCloudApple
//
//  Created by Deling Ren on 12/15/23.
//

import SwiftUI

struct UploadsView: View {
  @EnvironmentObject var model: Main

  var body: some View {
    Form {
      Button(action: { model.notifyReceiver() }) {
        Text("Upload")}
      List {
        Section {
          ForEach(model.outgoingPackets) { packet in
            HStack{
              Text("Packet")
              if packet.notificationToken != nil {
                Text(" w/ token")
              }
            }
          }
        } header: {
          Text("Uploads")
        }
      }
    }
  }
}

#Preview {
  UploadsView().environment(Main.createDebugModel())
}
