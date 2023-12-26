package com.google.hellocloud;

import androidx.databinding.BaseObservable;

import java.util.UUID;

public class File extends BaseObservable {
  public UUID id;
  public String mimeType;
  public long fileSize;

  @Override
  public String toString() {
    return String.format("%s, %.1f KB", mimeType, fileSize / 1024.0);
  }
}
