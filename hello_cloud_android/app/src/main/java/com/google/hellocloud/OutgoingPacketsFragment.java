package com.google.hellocloud;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListView;
import androidx.databinding.BindingAdapter;
import androidx.databinding.DataBindingUtil;
import androidx.fragment.app.Fragment;
import com.google.hellocloud.databinding.FragmentOutgoingPacketsBinding;
import java.util.List;

public class OutgoingPacketsFragment extends Fragment {
  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    MainActivity activity = (MainActivity) getActivity();
    activity.showBackButton();
    activity.setTitle("Outgoing");

    FragmentOutgoingPacketsBinding binding =
        DataBindingUtil.inflate(inflater, R.layout.fragment_outgoing_packets, container, false);
    binding.setModel(Main.shared);
    return binding.getRoot();
  }

  @BindingAdapter("entries")
  public static void setEntries(View view, List<Packet<OutgoingFile>> files) {
    Context context = view.getContext();
    PacketListAdapter<OutgoingFile> adapter =
        new PacketListAdapter<>(
            context, files, R.layout.item_outgoing_packet, R.layout.item_outgoing_file);
    ExpandableListView listView = (ExpandableListView) view;
    listView.setAdapter(adapter);
    for (int i = 0; i < adapter.getGroupCount(); i++) {
      listView.expandGroup(i);
    }
  }
}
