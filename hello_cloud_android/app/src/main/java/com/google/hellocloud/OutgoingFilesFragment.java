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

public class OutgoingFilesFragment extends ListOnEndpointFragment {
  ActivityResultLauncher<PickVisualMediaRequest> picker;

  public OutgoingFilesFragment() {
    super(R.layout.fragment_outgoing_files);
  }

  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    picker = registerForActivityResult(
            new ActivityResultContracts.PickMultipleVisualMedia(), this::onMediaPicked);

    View view = super.onCreateView(inflater, container, savedInstanceState);
    assert view != null;
    FragmentOutgoingFilesBinding binding = DataBindingUtil.getBinding(view);
    assert binding != null;

    binding.pick.setOnClickListener(v ->
            picker.launch(new PickVisualMediaRequest.Builder()
                    .setMediaType(ActivityResultContracts.PickVisualMedia.ImageAndVideo.INSTANCE)
                    .build()));
    binding.upload.setOnClickListener(v -> {
      for (OutgoingFileViewModel file : endpointViewModel.getOutgoingFiles()) {
        if (file.state == OutgoingFileViewModel.State.PICKED) {
          // TODO: time the upload
          file.upload();
          file.remotePath = "1234567890";
          // TODO: add a transfer
        }
      }
    });
    binding.send.setOnClickListener(v ->
            MainViewModel.shared.sendFiles(
                    endpointViewModel.id, endpointViewModel.getOutgoingFiles()));

    return view;
  }

  private void onMediaPicked(List<Uri> uris) {
    Context context = getView().getContext();
    ContentResolver resolver = context.getContentResolver();
    ArrayList<OutgoingFileViewModel> files = new ArrayList<>();
    for (Uri uri : uris) {
      String mimeType = resolver.getType(uri);

      // Get the local file name and size.
      Cursor cursor = resolver.query(uri,
              new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE},
              null, null, null);

      assert cursor != null;
      int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
      int sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE);
      cursor.moveToFirst();

      String name = cursor.getString(nameIndex);
      int size = cursor.getInt(sizeIndex);
      cursor.close();

      // Construct an outgoing file to be added to the endpoint's list
      OutgoingFileViewModel file = new OutgoingFileViewModel(mimeType, name, null, size, OutgoingFileViewModel.State.PICKED);
      files.add(file);
    }
    endpointViewModel.onMediaPicked(files);
  }

  private void pick(View view) {

  }

  private void upload(View view) {

  }

  private void send(View view) {
  }

  @BindingAdapter("entries")
  public static void setEntries(View view, List<OutgoingFileViewModel> files) {
    setEntries(R.layout.item_outgoing_file, BR.outgoingFileViewModel, view, files);
  }
}