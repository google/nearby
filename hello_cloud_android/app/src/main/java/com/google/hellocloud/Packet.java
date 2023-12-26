package com.google.hellocloud;

import java.util.ArrayList;
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

  public String getOutgoingDescription() {
    return "Packet" + (receiver == null ? "" : (" for " + receiver));
  }

  public String getIncomingDescription() {
    return "Packet" + (sender == null ? "" : (" from " + sender));
  }
}
