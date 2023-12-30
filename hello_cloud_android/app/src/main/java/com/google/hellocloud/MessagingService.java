package com.google.hellocloud;

import static com.google.hellocloud.Utils.TAG;

import android.util.Log;
import androidx.annotation.NonNull;
import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

public class MessagingService extends FirebaseMessagingService {
  @Override
  public void onNewToken(@NonNull String token) {
    Main.shared.notificationToken = token;
  }

  @Override
  public void onMessageReceived(RemoteMessage remoteMessage) {
    // Not getting messages here? See why this may be: https://goo.gl/39bRNJ
    Log.d(TAG, "Message from: " + remoteMessage.getFrom());

    // Check if message contains a data payload.
    if (remoteMessage.getData().size() > 0) {
      Log.d(TAG, "Message data payload: " + remoteMessage.getData());
    }

    // Check if message contains a notification payload.
    if (remoteMessage.getNotification() != null) {
      Log.d(TAG, "Message Notification Body: " + remoteMessage.getNotification().getBody());
    }

    // Also if you intend on generating your own notifications as a result of a received FCM
    // message, here is where that should be initiated. See sendNotification method below.
  }
}
