package com.google.hellocloud;

import static com.google.hellocloud.Utils.TAG;
import static com.google.hellocloud.Utils.logErrorAndToast;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.MediaStore;
import android.util.Log;
import androidx.databinding.BaseObservable;
import androidx.databinding.Bindable;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.UUID;

public class Packet<T extends File> extends BaseObservable {
  public enum State {
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
  private transient State state;
  public transient boolean highlighted = false;

  public Packet() {
    id = UUID.randomUUID();
  }

  public Packet(UUID uuid) {
    this.id = uuid;
  }

  @Bindable
  public State getState() {
    return state;
  }

  public Packet<T> setState(State state) {
    this.state = state;
    notifyPropertyChanged(BR.state);
    notifyPropertyChanged(BR.isBusy);
    notifyPropertyChanged(BR.outgoingStateIcon);
    notifyPropertyChanged(BR.incomingStateIcon);
    notifyPropertyChanged(BR.canUpload);
    notifyPropertyChanged(BR.canDownload);
    return this;
  }

  @Bindable
  public boolean getHighlighted() {
    return highlighted;
  }

  public void setHighlighted(boolean value) {
    this.highlighted = value;
    notifyPropertyChanged(BR.highlighted);
  }

  public String getOutgoingDescription() {
    return "Packet" + (receiver == null ? "" : (" for " + receiver));
  }

  public String getIncomingDescription() {
    return "Packet" + (sender == null ? "" : (" from " + sender));
  }

  @Bindable
  public boolean getIsBusy() {
    return state == State.DOWNLOADING || state == State.UPLOADING;
  }

  @Bindable
  public Drawable getOutgoingStateIcon() {
    if (state == null) {
      return null;
    }
    // LOADED: upload button
    // UPLOADED: green filled circle
    // UPLOADING: spinner
    int resource;
    switch (state) {
      case UPLOADED -> resource = R.drawable.uploaded_outgoing;
      default -> {
        return null;
      }
    }
    return Main.shared.context.getResources().getDrawable(resource, null);
  }

  @Bindable
  public Drawable getIncomingStateIcon() {
    if (state == null) {
      return null;
    }
    // RECEIVED: grey dotted circle
    // UPLOADED: download button
    // DOWNLOADING: spinner
    // DOWNLOADED: green filled circle
    int resource;
    switch (state) {
      case RECEIVED -> resource = R.drawable.received;
      case DOWNLOADED -> resource = R.drawable.downloaded;
      default -> {
        return null;
      }
    }
    return Main.shared.context.getResources().getDrawable(resource, null);
  }

  @Bindable
  public boolean getCanUpload() {
    return state == State.LOADED;
  }

  @Bindable
  public boolean getCanDownload() {
    return state == State.UPLOADED;
  }

  public void update(Packet<T> newPacket) {
    if (newPacket == null) {
      return;
    }

    for (T newFile : newPacket.files) {
      if (newFile instanceof IncomingFile) {
        var maybeOldFile = files.stream().filter(f -> f.id.equals(newFile.id)).findFirst();
        maybeOldFile.ifPresent(
            oldFile -> {
              ((IncomingFile) oldFile).remotePath = ((IncomingFile) newFile).remotePath;
              ((IncomingFile) oldFile).setState(IncomingFile.State.UPLOADED);
            });
      }
    }
    setState(State.UPLOADED);
  }

  public void download(Context context) {
    if (state != State.UPLOADED) {
      Log.e(TAG, "Packet is not uploaded before being downloaded.");
      return;
    }
    setState(State.DOWNLOADING);

    ArrayList<Task<Long>> tasks = new ArrayList<>();
    Instant beginTime = Instant.now();
    long totalSize = files.stream().map(file -> file.fileSize).reduce(0L, Long::sum);

    for (T file : files) {
      IncomingFile incomingFile = (IncomingFile) file;
      assert incomingFile != null;

      ContentResolver resolver = context.getContentResolver();
      String fileName = UUID.randomUUID().toString().toUpperCase();
      Uri imagesUri = MediaStore.Images.Media.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
      ContentValues values = new ContentValues();
      values.put(MediaStore.MediaColumns.DISPLAY_NAME, fileName);
      values.put(MediaStore.MediaColumns.MIME_TYPE, file.mimeType);
      Uri uri = resolver.insert(imagesUri, values);
      incomingFile.setLocalUri(uri);

      Task<Long> task =
          incomingFile
              .download()
              .addOnSuccessListener(
                  result -> {
                    setState(State.DOWNLOADED);
                    Instant endTime = Instant.now();
                    Duration duration = Duration.between(beginTime, endTime);
                    Log.i(
                        TAG,
                        String.format(
                            "Downloaded. Size(b): %s. Time(s): %d.",
                            incomingFile.fileSize, duration.getSeconds()));
                  })
              .addOnFailureListener(
                  error -> {
                    setState(State.UPLOADED);
                    logErrorAndToast(
                        Main.shared.context,
                        R.string.error_toast_cannot_download,
                        error.getMessage());
                  });
      tasks.add(task);
    }

    Tasks.whenAllSuccess(tasks)
        .addOnCompleteListener(
            downloadTask -> {
              if (downloadTask.isSuccessful()) {
                Instant endTime = Instant.now();
                double duration = (double) (Duration.between(beginTime, endTime).getSeconds());
                Log.i(
                    TAG,
                    String.format(
                        "Downloaded packet. Size(b): %d. Time(s): %.1f. Speed(KB/s): %.1f",
                        totalSize, duration, ((double) totalSize) / 1024.0 / duration));
              }
            });
  }

  public void upload() {
    if (state != State.LOADED) {
      Log.e(TAG, "Packet is not loaded before being uploaded.");
      return;
    }
    setState(State.UPLOADING);

    ArrayList<Task<Long>> tasks = new ArrayList<>();
    Instant beginTime = Instant.now();
    long totalSize = files.stream().map(file -> file.fileSize).reduce(0L, Long::sum);
    for (T file : files) {
      OutgoingFile outgoingFile = (OutgoingFile) file;
      assert (outgoingFile != null);
      Task<Long> task =
          outgoingFile
              .upload()
              .addOnSuccessListener(
                  result -> {
                    setState(State.UPLOADED);
                    Instant endTime = Instant.now();
                    Duration duration = Duration.between(beginTime, endTime);
                    Log.i(
                        TAG,
                        String.format(
                            "Uploaded file. Size(b): %s. Time(s): %d.",
                            outgoingFile.fileSize, duration.getSeconds()));
                  })
              .addOnFailureListener(
                  error -> {
                    setState(State.LOADED);
                    logErrorAndToast(
                        Main.shared.context,
                        R.string.error_toast_cannot_upload,
                        error.getMessage());
                  });
      tasks.add(task);
    }

    Tasks.whenAllSuccess(tasks)
        .addOnCompleteListener(
            uploadTask -> {
              if (uploadTask.isSuccessful()) {
                Instant endTime = Instant.now();
                double duration = (double) (Duration.between(beginTime, endTime).getSeconds());
                Log.i(
                    TAG,
                    String.format(
                        "Uploaded packet. Size(b): %d. Time(s): %.1f. Speed(KB/s): %.1f",
                        totalSize, duration, ((double) totalSize) / 1024.0 / duration));
                CloudDatabase.shared
                    .push((Packet<OutgoingFile>) this)
                    .addOnCompleteListener(
                        pushTask -> {
                          if (pushTask.isSuccessful()) {
                            Log.i(TAG, "Pushed packet " + id);
                            state = State.UPLOADED;
                          } else {
                            Log.e(TAG, "Failed to push packet " + id);
                            state = State.LOADED;
                          }
                        });
              }
            });
  }
}
