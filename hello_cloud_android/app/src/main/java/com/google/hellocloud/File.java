package com.google.hellocloud;

import androidx.databinding.BaseObservable;

import java.util.UUID;

public class File extends BaseObservable {
  public UUID id;
  public String mimeType;
  public long fileSize;
}
