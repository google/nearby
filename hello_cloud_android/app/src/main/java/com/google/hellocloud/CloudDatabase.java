package com.google.hellocloud;

import com.google.android.gms.tasks.Task;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonPrimitive;
import java.util.ArrayList;
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

  private Object jsonToObject(JsonElement element) {
    if (element.isJsonPrimitive()) {
      // We really just need element.value. But JsonPrimitive does not provide an accessor.
      // I *think* only numbers need to be treated specially because we don't want quotes.
      JsonPrimitive primitive = element.getAsJsonPrimitive();
      if (primitive.isNumber()) {
        return primitive.getAsNumber();
      } else {
        return primitive.getAsString();
      }
    }
    if (element.isJsonObject()) {
      Map<String, Object> result = new HashMap<>();
      Map<String, JsonElement> map = element.getAsJsonObject().asMap();
      for (Map.Entry<String, JsonElement> entry : map.entrySet()) {
        String key = entry.getKey();
        JsonElement value = entry.getValue();
        if (value != null && !value.isJsonNull()) {
          result.put(key, jsonToObject(value));
        }
      }
      return result;
    }
    if (element.isJsonArray()) {
      ArrayList<Object> result = new ArrayList<>();
      JsonArray array = element.getAsJsonArray();
      for (var item : array) {
        result.add(jsonToObject(item));
      }
      return result;
    }
    return null;
  }

  private Map<String, Object> serializePacket(Packet<OutgoingFile> packet) {
    JsonElement jsonTree = DataWrapper.getGson().toJsonTree(packet);
    Object object = jsonToObject(jsonTree);
    return (Map<String, Object>) object;
  }

  public Task<Void> push(Packet<OutgoingFile> packet) {
    Map<String, Object> packetData = serializePacket(packet);
    // Use upper case for packet IDs. iOS does the same
    return databaseRef
        .child("packets/" + packet.id.toString().toUpperCase())
        .updateChildren(packetData);
  }
}
