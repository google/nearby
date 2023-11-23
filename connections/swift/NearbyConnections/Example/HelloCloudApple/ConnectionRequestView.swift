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

struct ConnectionRequestView: View {
    var connectionRequest: ConnectionRequest
    var onAcceptConnection : () -> ()
    var onRejectConnection : () -> ()

    @State private var hasResponded = false

    var body: some View {
        Section(footer: Text("Security code: \(connectionRequest.pin)")) {
            Button("Accept") {
                hasResponded = true
                onAcceptConnection()
            }.disabled(hasResponded)
            Button("Reject", role: .destructive) {
                hasResponded = true
                onRejectConnection()
            }.disabled(hasResponded)
        }
    }
}

struct ConnectionRequestView_Previews: PreviewProvider {
    static var previews: some View {
        Form {
            ConnectionRequestView(
                connectionRequest: ConnectionRequest(
                    id: UUID(),
                    endpointID: "",
                    endpointName: "Example",
                    pin: "1234",
                    shouldAccept: { _ in }
                ),
                onAcceptConnection: {},
                onRejectConnection: {}
            )
        }
    }
}
