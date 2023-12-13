package com.google.hellocloud;

import android.graphics.drawable.Drawable;

import java.time.Duration;

public class TransferViewModel {
  enum Direction {
    SEND, RECEIVE, UPLOAD, DOWNLOAD
  }

  enum Result {
    SUCCESS, FAILURE, CANCELED
  }

  public Direction direction;
  public String remotePath;
  public Result result;
  public int size;
  public Duration duration;
  public String from;
  public String to;

  public Drawable getDirectionIcon() {
    int resource;
    switch (direction) {
      case SEND:
        resource = R.drawable.send;
        break;
      case RECEIVE:
        resource = R.drawable.receive;
        break;
      case UPLOAD:
        resource = R.drawable.upload;
        break;
      case DOWNLOAD:
        resource = R.drawable.download;
        break;
      default:
        throw new RuntimeException("Unknown state of transfer.");
    }
    return MainViewModel.shared.context.getResources().getDrawable(resource, null);
  }

  public Drawable getResultIcon() {
    int resource;
    switch (result) {
      case SUCCESS:
        resource = R.drawable.success;
        break;
      case FAILURE:
        resource = R.drawable.failure;
        break;
      case CANCELED:
        resource = R.drawable.canceled;
        break;
      default:
        throw new RuntimeException("Unknown result of transfer.");
    }
    return MainViewModel.shared.context.getResources().getDrawable(resource, null);
  }

  public TransferViewModel(Direction direction, String remotePath, Result result, int size, Duration duration, String from, String to) {
    this.direction = direction;
    this.remotePath = remotePath;
    this.result = result;
    this.size = size;
    this.duration = duration;
    this.from = from;
    this.to = to;
  }

  public static TransferViewModel send(String remotePath, Result result, String to) {
    return new TransferViewModel(Direction.SEND, remotePath, result, 0, Duration.ZERO, null, to);
  }

  public static TransferViewModel receive(String remotePath, Result result, String from) {
    return new TransferViewModel(Direction.RECEIVE, remotePath, result, 0, Duration.ZERO, from, null);
  }

  public static TransferViewModel download(String remotePath, Result result, int size, Duration duration) {
    return new TransferViewModel(Direction.DOWNLOAD, remotePath, result, size, duration, null, null);
  }

  public static TransferViewModel upload(String remotePath, Result result, int size, Duration duration) {
    return new TransferViewModel(Direction.UPLOAD, remotePath, result, size, duration, null, null);
  }
}

