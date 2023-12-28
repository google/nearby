package com.google.hellocloud;

import static com.google.hellocloud.Utils.TAG;

import android.util.Log;
import androidx.annotation.NonNull;
import com.google.android.gms.tasks.Continuation;
import com.google.android.gms.tasks.Task;
import com.google.firebase.database.DataSnapshot;
import com.google.firebase.database.DatabaseError;
import com.google.firebase.database.DatabaseReference;
import com.google.firebase.database.FirebaseDatabase;
import com.google.firebase.database.ValueEventListener;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.function.Function;

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
      for (JsonElement item : array) {
        result.add(jsonToObject(item));
      }
      return result;
    }
    return null;
  }

  private JsonElement objectToJson(Object src) {
    if (src instanceof Number) {
      return new JsonPrimitive((Number) src);
    }
    if (src instanceof Boolean) {
      return new JsonPrimitive((Boolean) src);
    }
    if (src instanceof Map) {
      Map<String, Object> map = (Map<String, Object>) src;
      JsonObject object = new JsonObject();
      for (Map.Entry<String, Object> entry : map.entrySet()) {
        String key = entry.getKey();
        Object value = entry.getValue();
        if (key == null || value == null) {
          return null;
        }
        object.add(key, objectToJson(value));
      }
      return object;
    }
    if (src instanceof List) {
      JsonArray array = new JsonArray();
      for (Object item : (List<Object>) src) {
        array.add(objectToJson(item));
      }
      return array;
    }
    return new JsonPrimitive(src.toString());
  }

  private Map<String, Object> serializePacket(Packet<OutgoingFile> packet) {
    JsonElement jsonTree = DataWrapper.getGson().toJsonTree(packet);
    Object object = jsonToObject(jsonTree);
    return (Map<String, Object>) object;
  }

  private Packet<IncomingFile> deserializePacket(Map<String, Object> map) {
    JsonElement jsonTree = objectToJson(map);
    Packet<IncomingFile> packet = DataWrapper.getGson().fromJson(jsonTree, Packet.class);
    return packet;
  }

  public Task<Void> push(Packet<OutgoingFile> packet) {
    Map<String, Object> packetData = serializePacket(packet);
    // Use upper case for packet IDs. iOS does the same
    return databaseRef
        .child("packets/" + packet.id.toString().toUpperCase())
        .updateChildren(packetData);
  }

  public Packet<IncomingFile> readPacket(DataSnapshot snapshot) {
    Map<String, Object> map = (Map<String, Object>) snapshot.getValue();
    if (map == null) {
      Log.e(TAG, "Failed to read from the snapshot");
      return null;
    }
    Packet<IncomingFile> packet = deserializePacket(map);
    if (packet == null) {
      Log.e(TAG, "Failed to decode the packet from the snapshot");
      return null;
    }
    return packet;
  }

  public void observePacket(UUID id, Function<Packet<IncomingFile>, Void> notification) {
    databaseRef
        .child("packets/" + id.toString().toUpperCase())
        .addValueEventListener(
            new ValueEventListener() {
              @Override
              public void onDataChange(@NonNull DataSnapshot snapshot) {
                if (snapshot.getValue() == null) {
                  return;
                }
                Packet<IncomingFile> packet = readPacket(snapshot);
                if (packet != null) {
                  notification.apply(packet);
                }
              }

              @Override
              public void onCancelled(@NonNull DatabaseError error) {}
            });
  }

  Task<Packet<IncomingFile>> pull(UUID packetId) {
    return databaseRef
        .child("packets/" + packetId.toString().toUpperCase())
        .get()
        .continueWith(
            (Continuation<DataSnapshot, Packet<IncomingFile>>)
                task -> {
                  if (!task.isSuccessful()) {
                    Log.e(TAG, "Failed to read snapshot from database");
                    return null;
                  }
                  DataSnapshot snapshot = task.getResult();
                  return readPacket(snapshot);
                });
  }
}
