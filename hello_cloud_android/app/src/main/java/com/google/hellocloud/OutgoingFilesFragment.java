package com.google.hellocloud;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.PickVisualMediaRequest;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.databinding.BindingAdapter;
import androidx.databinding.DataBindingUtil;
import com.google.hellocloud.databinding.FragmentOutgoingFilesBinding;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class OutgoingFilesFragment extends ListOnEndpointFragment {
  ActivityResultLauncher<PickVisualMediaRequest> picker;

  public OutgoingFilesFragment() {
    super(R.layout.fragment_outgoing_files);
  }

  @Override
  public View onCreateView(
      @NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    picker =
        registerForActivityResult(
            new ActivityResultContracts.PickMultipleVisualMedia(), this::onMediaPicked);

    View view = super.onCreateView(inflater, container, savedInstanceState);
    assert view != null;
    FragmentOutgoingFilesBinding binding = DataBindingUtil.getBinding(view);
    assert binding != null;

    binding.pick.setOnClickListener(v -> pick());
    binding.upload.setOnClickListener(v -> upload());
    binding.send.setOnClickListener(v -> endpointViewModel.sendFiles());

    return view;
  }

  private void onMediaPicked(List<Uri> uris) {
    Context context = getView().getContext();
    ContentResolver resolver = context.getContentResolver();
    ArrayList<OutgoingFileViewModel> files = new ArrayList<>();
    for (Uri uri : uris) {
      String mimeType = resolver.getType(uri);

      // Get the local file name and size.
      Cursor cursor =
          resolver.query(
              uri,
              new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE},
              null,
              null,
              null);

      assert cursor != null;
      int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
      int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);
      cursor.moveToFirst();

      String name = cursor.getString(nameIndex);
      int size = cursor.getInt(sizeIndex);
      cursor.close();

      // Construct an outgoing file to be added to the endpoint's list
      OutgoingFileViewModel file =
          new OutgoingFileViewModel(mimeType, name, null, size)
              .setState(OutgoingFileViewModel.State.PICKED)
              .setLocalUri(uri);
      files.add(file);
    }
    endpointViewModel.onMediaPicked(files);
  }

  private void pick() {
    assert picker != null;
    picker.launch(
        new PickVisualMediaRequest.Builder()
            .setMediaType(ActivityResultContracts.PickVisualMedia.ImageAndVideo.INSTANCE)
            .build());
  }

  private void upload() {
    for (OutgoingFileViewModel file : endpointViewModel.getOutgoingFiles()) {
      if (file.getState() == OutgoingFileViewModel.State.PICKED) {
        // TODO: time the upload
        file.remotePath = UUID.randomUUID().toString();
        file.upload();
        // TODO: add a transfer
      }
    }
    /*
     func upload() -> Void {
       for file in model.outgoingFiles {
         if file.state == .loaded {
           let beginTime = Date()
           file.upload() { [beginTime] size, error in
             let duration: TimeInterval = Date().timeIntervalSince(beginTime)
             model.transfers.append(
               Transfer(
                 direction: .upload,
                 remotePath: file.remotePath!,
                 result: .success,
                 size: Int(file.fileSize),
                 duration: duration))
           }
         }
       }
     }
    */
  }

  @BindingAdapter("entries")
  public static void setEntries(View view, List<OutgoingFileViewModel> files) {
    setEntries(R.layout.item_outgoing_file, BR.outgoingFileViewModel, view, files);
  }
}
