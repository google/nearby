package com.google.hellocloud;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListView;
import androidx.annotation.Nullable;
import androidx.databinding.BindingAdapter;
import androidx.databinding.DataBindingUtil;
import androidx.fragment.app.Fragment;
import com.google.hellocloud.databinding.FragmentIncomingPacketsBinding;
import java.util.List;

public class IncomingPacketsFragment extends Fragment {
  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    MainActivity activity = (MainActivity) getActivity();
    activity.showBackButton();
    activity.setTitle("Incoming");

    FragmentIncomingPacketsBinding binding =
        DataBindingUtil.inflate(inflater, R.layout.fragment_incoming_packets, container, false);
    binding.setModel(Main.shared);
    binding.refresher.setOnRefreshListener(
        () -> {
          binding.refresher.setRefreshing(false);
          for (var packet : Main.shared.getIncomingPackets()) {
            if (packet.getState() == Packet.State.UPLOADED) {
              continue;
            }
            CloudDatabase.shared.pull(packet.id).addOnSuccessListener(packet::update);
          }
        });

    return binding.getRoot();
  }

  @BindingAdapter("entries")
  public static void setEntries(View view, List<Packet<IncomingFile>> packets) {
    Context context = view.getContext();
    PacketListAdapter<IncomingFile> adapter =
        new PacketListAdapter<>(
            context, packets, R.layout.item_incoming_packet, R.layout.item_incoming_file);
    ExpandableListView listView = (ExpandableListView) view;
    listView.setAdapter(adapter);
    for (int i = 0; i < adapter.getGroupCount(); i++) {
      listView.expandGroup(i);
    }
  }
}
