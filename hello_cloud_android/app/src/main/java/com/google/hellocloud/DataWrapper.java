package com.google.hellocloud;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonDeserializationContext;
import com.google.gson.JsonDeserializer;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParseException;
import com.google.gson.JsonPrimitive;
import com.google.gson.JsonSerializationContext;
import com.google.gson.JsonSerializer;
import com.google.gson.reflect.TypeToken;

import java.lang.reflect.Type;
import java.util.ArrayList;
import java.util.Map;
import java.util.Objects;

class DataWrapper<T extends File> {
  enum Kind {
    PACKET,
    NOTIFICATION_TOKEN
  }

  public Kind kind;
  public Packet<T> packet;
  public String notificationToken;

  public DataWrapper(Packet<T> packet) {
    this.kind = Kind.PACKET;
    this.packet = packet;
  }

  public DataWrapper(String notificationToken) {
    this.kind = Kind.NOTIFICATION_TOKEN;
    this.notificationToken = notificationToken;
  }

  public static class Serializer implements JsonSerializer<DataWrapper<OutgoingFile>> {
    @Override
    public JsonElement serialize(
        DataWrapper<OutgoingFile> src, Type typeOfSrc, JsonSerializationContext context) {
      GsonBuilder gson = new GsonBuilder();
      JsonObject result = new JsonObject();

      if (src.kind == Kind.PACKET) {
        gson.registerTypeAdapter(ArrayList.class, new OutgoingFilesSerializer());
        result.add("type", new JsonPrimitive("packet"));
        result.add("data", gson.create().toJsonTree(src.packet));
      } else if (src.kind == Kind.NOTIFICATION_TOKEN) {
        result.add("type", new JsonPrimitive("notificationToken"));
        result.add("data", new JsonPrimitive(src.notificationToken));
      }
      return result;
    }

    public static class OutgoingFilesSerializer implements JsonSerializer<ArrayList<OutgoingFile>> {
      @Override
      public JsonElement serialize(
          ArrayList<OutgoingFile> src, Type typeOfSrc, JsonSerializationContext context) {
        JsonObject result = new JsonObject();
        for (OutgoingFile file : src) {
          result.add(file.id.toString(), context.serialize(file));
        }
        return result;
      }
    }
  }

  public static class Deserializer implements JsonDeserializer<DataWrapper<IncomingFile>> {
    @Override
    public DataWrapper<IncomingFile> deserialize(
        JsonElement json, Type typeOfT, JsonDeserializationContext context)
        throws JsonParseException {
      var jsonObject = (JsonObject) json;
      if (jsonObject == null) {
        return null;
      }

      String kind = jsonObject.get("type").getAsString();
      JsonElement data = jsonObject.get("data");
      if (Objects.equals(kind, "packet")) {
        GsonBuilder gson = new GsonBuilder();
        gson.registerTypeAdapter(ArrayList.class, new Deserializer.IncomingFilesDeserializer());
        var packet = (Packet<IncomingFile>) gson.create().fromJson(data, Packet.class);
        return new DataWrapper<>(packet);
      } else if (Objects.equals(kind, "notificationToken")) {
        var notificationToken = (String) context.deserialize(data, String.class);
        return new DataWrapper<>(notificationToken);
      }
      return null;
    }

    public static class IncomingFilesDeserializer
        implements JsonDeserializer<ArrayList<IncomingFile>> {

      @Override
      public ArrayList<IncomingFile> deserialize(
          JsonElement json, Type typeOfT, JsonDeserializationContext context)
          throws JsonParseException {
        var jsonArray = (JsonObject) json;
        if (jsonArray == null) {
          return null;
        }
        TypeToken<Map<String, IncomingFile>> mapType = new TypeToken<>(){};
        Gson gson = new Gson();
        Map<String, IncomingFile> map = gson.fromJson(json, mapType);
        return new ArrayList<>(map.values());
      }
    }
  }
}
