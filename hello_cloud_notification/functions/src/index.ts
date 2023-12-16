/**
 * Import function triggers from their respective submodules:
 *
 * import {onCall} from "firebase-functions/v2/https";
 * import {onDocumentWritten} from "firebase-functions/v2/firestore";
 *
 * See a full list of supported triggers at https://firebase.google.com/docs/functions
 */

import { onRequest } from "firebase-functions/v2/https";
import * as logger from "firebase-functions/logger";
import * as admin from "firebase-admin";
import { Message } from "firebase-admin/lib/messaging/messaging-api";

// Start writing functions
// https://firebase.google.com/docs/functions/typescript

admin.initializeApp();

export const sendNotification = onRequest((request, response) => {
  const message: Message = {
    token: "dUcjcnLNZ0hxuqWScq2UDh:APA91bGG8GTykBZgAkGA_xkBVnefjUb-PvR4mDNjwjv1Sv7EYGZc89zyfoy6Syz63cQ3OkQUH3D5Drf0674CZOumgBsgX8sR4JGQANWeFNjC_RScHWDyA8ZhYdzHdp7t6uQjqEhF_TEL",
    notification: {
      title: "You've got files!",
      body: "Your files are ready for downloading",
    },
    data: { foo: "bar" }
  };

  admin.messaging().send(message)
    .then((result) => {
      logger.info("Notification sent", { structuredData: true });
      response.json({ data: { success: "true" } });
    })
    .catch((error) => {
      logger.info("Notification failed to send", { structuredData: true });
      response.status(500).json({ error: error });
    });
});
