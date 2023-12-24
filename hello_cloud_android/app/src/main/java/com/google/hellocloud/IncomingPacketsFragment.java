package com.google.hellocloud;

import android.os.Bundle;
import androidx.fragment.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

public class IncomingPacketsFragment extends Fragment {

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        MainActivity activity = (MainActivity)getActivity();
        activity.showBackButton();
        activity.setTitle("Incoming");
        return inflater.inflate(R.layout.fragment_incoming_packets, container, false);
    }
}