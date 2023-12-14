package com.google.hellocloud;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;
import androidx.annotation.NonNull;
import androidx.databinding.Bindable;
import androidx.databinding.BindingAdapter;
import androidx.databinding.DataBindingUtil;
import com.google.hellocloud.databinding.FragmentIncomingFilesBinding;

import java.io.OutputStream;
import java.util.List;
import java.util.UUID;

import android.content.ContentValues;

public class IncomingFilesFragment extends ListOnEndpointFragment {
  public IncomingFilesFragment() {
    super(R.layout.fragment_incoming_files);
  }

  @Override
  public View onCreateView(
      @NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    View view = super.onCreateView(inflater, container, savedInstanceState);
    FragmentIncomingFilesBinding binding = DataBindingUtil.getBinding(view);
    binding.download.setOnClickListener(v -> downloadFiles());
    return view;
  }

  private void downloadFiles() {
    Context context = getView().getContext();
    ContentResolver resolver = context.getContentResolver();
    for (IncomingFileViewModel file : endpointViewModel.getIncomingFiles()) {
      if (file.getState() != IncomingFileViewModel.State.RECEIVED) {
        continue;
      }

      // Generate a random display name. We don't honor the sender's file name
      String fileName = UUID.randomUUID().toString().toUpperCase();
      Uri imagesUri = MediaStore.Images.Media.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY);
      ContentValues values = new ContentValues();
      values.put(MediaStore.MediaColumns.DISPLAY_NAME, fileName);
      values.put(MediaStore.MediaColumns.MIME_TYPE, file.mimeType);
      Uri uri = resolver.insert(imagesUri, values);
      file.setLocalUri(uri);
    }
    endpointViewModel.downloadFiles();
  }

  @BindingAdapter("entries")
  public static void setEntries(View view, List<IncomingFileViewModel> files) {
    Context context = view.getContext();
    SimpleArrayAdapter<IncomingFileViewModel> adapter =
        new SimpleArrayAdapter<IncomingFileViewModel>(
            R.layout.item_incoming_file, BR.incomingFileViewModel, context, files) {
          @NonNull
          @Override
          public View getView(int position, View convertView, ViewGroup parent) {
            View view = super.getView(position, convertView, parent);
            view.setOnClickListener(
                v -> {
                  IncomingFileViewModel file = getItem(position);
                  assert file != null;
                  System.out.println("Clicked: " + file.fileName);
                });
            return view;
          }
        };
    ((ListView) view).setAdapter(adapter);
  }
}
