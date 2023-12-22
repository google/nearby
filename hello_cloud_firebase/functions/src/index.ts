// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import { onValueCreated } from "firebase-functions/v2/database";
import * as logger from "firebase-functions/logger";
import * as admin from "firebase-admin";
import { Message } from "firebase-admin/lib/messaging/messaging-api";

admin.initializeApp();

export const onPacketCreated = onValueCreated(
  {
    ref: "/packets/{packetId}",
    instance: "hello-cloud-5b73c-default-rtdb",
  },
  (event) => {
    const packet = event.data.val();
    logger.info("Packet created: " + packet.packetId);
    if (packet.notificationToken == null) {
      logger.info("Packet has no notification token.");
      return;
    }

    logger.info("Notification token: " + packet.notificationToken);

    const message: Message = {
      token: packet.notificationToken,
      notification: {
        title: "You've got files!",
        body: "Your files from " + packet.sender + " are ready for downloading",
      },
      data: {
        packetId: packet.packetId,
      },
    };

    admin.messaging().send(message)
      .then((result) => {
        logger.info("Notification sent " + result, { structuredData: true });
      })
      .catch((error) => {
        logger.info("Notification failed to send " + error,
          { structuredData: true });
      });
  });
