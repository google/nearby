package com.google.hellocloud;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.fragment.app.Fragment;

public class OutgoingPacketsFragment extends Fragment {
  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    MainActivity activity = (MainActivity)getActivity();
    activity.showBackButton();
    activity.setTitle("Outgoing");
    return inflater.inflate(R.layout.fragment_outgoing_packets, container, false);
  }
}
