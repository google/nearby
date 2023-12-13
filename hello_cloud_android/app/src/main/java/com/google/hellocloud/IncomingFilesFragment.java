package com.google.hellocloud;

import static com.google.hellocloud.Util.logErrorAndToast;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import androidx.annotation.NonNull;
import androidx.databinding.BindingAdapter;
import androidx.databinding.DataBindingUtil;

import com.google.hellocloud.databinding.FragmentIncomingFilesBinding;

import java.time.Duration;
import java.time.Instant;
import java.util.List;

public class IncomingFilesFragment extends ListOnEndpointFragment {
  public IncomingFilesFragment() {
    super(R.layout.fragment_incoming_files);
  }

  @Override
  public View onCreateView(
      @NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    View view = super.onCreateView(inflater, container, savedInstanceState);
    FragmentIncomingFilesBinding binding = DataBindingUtil.getBinding(view);
    binding.download.setOnClickListener(v -> download());
    return view;
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

  private void download() {
    for (IncomingFileViewModel file : endpointViewModel.getIncomingFiles()) {
      if (file.getState() == IncomingFileViewModel.State.RECEIVED) {
        Instant beginTime = Instant.now();
        file.download(
            result -> {
              Instant endTime = Instant.now();
              Duration duration = Duration.between(beginTime, endTime);
              TransferViewModel transfer =
                  TransferViewModel.download(
                      file.remotePath, TransferViewModel.Result.SUCCESS, file.fileSize, duration);
              endpointViewModel.addTransfer(transfer);
            },
            exception -> {
              System.out.println(exception);
              logErrorAndToast(
                  MainViewModel.shared.context, R.string.error_toast_cannot_download, exception);
            });
      }
    }
  }
}
