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

import { onRequest } from "firebase-functions/v2/https";
import { onValueCreated } from "firebase-functions/v2/database";
import * as logger from "firebase-functions/logger";
import * as admin from "firebase-admin";
import { Message } from "firebase-admin/lib/messaging/messaging-api";

admin.initializeApp();

export const onPacketCreated = onValueCreated(
  {
    ref: "/packets/{id}",
    instance: "hello-cloud-5b73c-default-rtdb",
  },
  (event) => {
    const packet = event.data.val();
    logger.info("Packet created: " + packet.id);
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
        packetId: packet.id,
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

export const sendTestNotification = onRequest((request, response) => {
  const token: string = "dG2_mhhXQNWKncv97NSSmG:APA91bGz3oKnfqQWr2fXzZXrVBGbZCa963K8gs6pSLJO1yhB5rZ4p922t0qtmcI0h3zUGeNaR60g_eBLq6qhvvHdzxYJ0rmv6E-byLOAZRGvFlWr0AK_GuF5r2Q4Zi0qQ1Rf3DLZnx5d";
  // const token: string = "eVBltbRxQ8i-O5BHuH83xg:APA91bESqC6sm3seV33RIbHyIDQscLjleDPMs5UMLz5R4gYZqQv78DdNhiIQWbLAUtr7xdGV7Z3Js24cUCDEE5RqHJ7NtkLuufxnR0HX0OxvdeFHzcvSPsu1L2b5sA4nBDCaHEsLrEf8";
  const id: string = "117442B8-CD26-4E13-B233-3678C339BDBD";
  const message: Message = {
    token: token,
    notification: {
      title: "You've got files!",
      body: "Your files are ready for downloading",
    },
    data: {
      packetId: id,
    }
  };

  admin.messaging().send(message)
    .then((result) => {
      logger.info("Test notification sent " + result, { structuredData: true });
      response.json({ data: { success: "true" } });
    })
    .catch((error) => {
      logger.info("Test notification failed to send " + error, { structuredData: true });
      response.status(500).json({ error: error });
    });
});