package com.google.hellocloud;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import androidx.annotation.NonNull;
import androidx.databinding.BindingAdapter;

import java.util.List;

public class IncomingFilesFragment extends ListOnEndpointFragment {
  public IncomingFilesFragment() {
    super(R.layout.fragment_incoming_files);
  }

  @BindingAdapter("entries")
  public static void setEntries(View view, List<IncomingFileViewModel> files) {
    Context context = view.getContext();
    SimpleArrayAdapter<IncomingFileViewModel> adapter =
            new SimpleArrayAdapter<IncomingFileViewModel>
                    (R.layout.item_incoming_file, BR.incomingFileViewModel, context, files) {
              @NonNull
              @Override
              public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);
                view.setOnClickListener(v -> {
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