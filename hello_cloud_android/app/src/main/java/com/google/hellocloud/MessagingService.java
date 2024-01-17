package com.google.hellocloud;

import static android.app.PendingIntent.FLAG_UPDATE_CURRENT;
import static com.google.hellocloud.Utils.TAG;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.core.app.NotificationCompat;
import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;
import java.util.Random;

public class MessagingService extends FirebaseMessagingService {
  @Override
  public void onNewToken(@NonNull String token) {
    Main.shared.notificationToken = token;
  }

  @Override
  public void onMessageReceived(RemoteMessage remoteMessage) {
    // Not getting messages here? See why this may be: https://goo.gl/39bRNJ
    Log.d(TAG, "Message from: " + remoteMessage.getFrom());

    String packetId = remoteMessage.getData().get("packetId");
    if (packetId == null) {
      return;
    }

    RemoteMessage.Notification notification = remoteMessage.getNotification();
    if (notification != null) {
      String title = notification.getTitle();
      String body = notification.getBody();

      Intent intent = new Intent(this, MainActivity.class);
      intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
      intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
      intent.putExtra("packetId", packetId);

      PendingIntent pendingIntent =
          PendingIntent.getActivity(
              this, 0, intent, PendingIntent.FLAG_MUTABLE | FLAG_UPDATE_CURRENT);
      NotificationCompat.Builder builder =
          new NotificationCompat.Builder(this, "DEFAULT_CHANNEL")
              .setSmallIcon(R.drawable.ic_launcher_foreground)
              .setContentTitle(title)
              .setContentText(body)
              .setColor(getResources().getColor(R.color.packet_highlight))
              .setContentIntent(pendingIntent)
              .setPriority(NotificationCompat.PRIORITY_MAX);

      NotificationManager notificationManager =
          (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
      notificationManager.notify(new Random().nextInt(), builder.build());
    }
  }
}
