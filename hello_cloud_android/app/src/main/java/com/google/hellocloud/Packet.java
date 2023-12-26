package com.google.hellocloud;

import com.google.gson.Gson;
import com.google.gson.JsonDeserializer;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import com.google.gson.JsonSerializationContext;
import com.google.gson.JsonSerializer;
import java.lang.reflect.Type;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class Packet<T extends File> {
  enum State {
    UNKNOWN,
    LOADED,
    UPLOADING,
    UPLOADED,
    RECEIVED,
    DOWNLOADING,
    DOWNLOADED
  }

  public UUID id;
  public String notificationToken;
  public ArrayList<T> files = new ArrayList<>();
  public String receiver;
  public String sender;

  public transient State state;

  public Packet() {
    id = UUID.randomUUID();
  }

  public Packet(UUID uuid) {
    this.id = uuid;
  }
}
