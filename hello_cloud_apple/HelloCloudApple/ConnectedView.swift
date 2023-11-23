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

struct ConnectedView: View {
    var connection: ConnectedEndpoint
    var onSendBytes : () -> ()
    var onDisconnect : () -> ()

    var body: some View {
        Section("Connection Actions") {
            Button("Send Bytes") {
                onSendBytes()
            }
            Button("Disconnect", role: .destructive) {
                onDisconnect()
            }
        }

        if !connection.payloads.isEmpty {
            Section(header: Text("Payloads")) {
                ForEach(connection.payloads) { payload in
                    PayloadView(payload: payload)
                }
            }
        }
    }
}

struct ConnectedView_Previews: PreviewProvider {
    static var previews: some View {
        Form {
            ConnectedView(
                connection: ConnectedEndpoint(
                    id: UUID(),
                    endpointID: "",
                    endpointName: "Example"
                ),
                onSendBytes: {},
                onDisconnect: {}
            )
        }
    }
}
