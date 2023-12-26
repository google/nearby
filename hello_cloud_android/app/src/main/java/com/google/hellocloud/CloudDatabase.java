package com.google.hellocloud;

import com.google.android.gms.tasks.Task;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;

import java.util.HashMap;
import java.util.Map;

public class CloudDatabase {
  public static CloudDatabase shared = new CloudDatabase();

  FirebaseDatabase database;
  DatabaseReference databaseRef;

  public CloudDatabase() {
    database = FirebaseDatabase.getInstance();
    if (Config.localCloud) {
      database.useEmulator(Config.localCloudHost, 9000);
    }
    databaseRef = database.getReference();
  }

  public Task<Void> push(Packet<OutgoingFile> packet) {
    Map<String, Object> packetData = new HashMap<>();
    packetData.put("id", packet.id.toString());
    return databaseRef.child("packets/" + packet.id).updateChildren(packetData);
  }
}
