package com.google.hellocloud;

import androidx.databinding.BindingAdapter;

import android.view.View;

import com.google.hellocloud.databinding.FragmentOutgoingFilesBinding;
import com.google.hellocloud.databinding.FragmentTransfersBinding;

import java.util.List;

public class TransfersFragment extends ListOnEndpointFragment {
  public TransfersFragment() {
    super(R.layout.fragment_transfers);
  }

  @BindingAdapter("entries")
  public static void setEntries(View view, List<TransferViewModel> transfers) {
    setEntries(R.layout.item_transfer, BR.transferViewModel, view, transfers);
  }
}