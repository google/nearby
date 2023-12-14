package com.google.hellocloud;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.databinding.BindingAdapter;
import androidx.databinding.DataBindingUtil;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.navigation.NavController;
import androidx.navigation.fragment.NavHostFragment;

import com.google.hellocloud.databinding.FragmentEndpointBinding;

import java.util.List;

public class EndpointFragment extends Fragment {
  private EndpointViewModel viewModel;

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
                           Bundle savedInstanceState) {
    // Inflate the layout for this fragment
    FragmentEndpointBinding binding = DataBindingUtil.inflate(
            inflater, R.layout.fragment_endpoint, container, false);

    String id = getArguments().getString("id");
    viewModel = MainViewModel.shared.getEndpoint(id).get();
    binding.setEndpointViewModel(viewModel);

    binding.connect.setOnClickListener(v -> {
      viewModel.setState(EndpointViewModel.State.PENDING);
      MainViewModel.shared.requestConnection(viewModel.id);
    });

    binding.disconnect.setOnClickListener(v -> {
      viewModel.setState(EndpointViewModel.State.PENDING);
      MainViewModel.shared.disconnect(viewModel.id);
    });

    binding.outgoingFiles.setOnClickListener(v -> navigate(R.id.action_endpointFragment_to_outgoingFilesFragment));
    binding.incomingFiles.setOnClickListener(v -> navigate(R.id.action_endpointFragment_to_incomingFilesFragment));
    binding.transfers.setOnClickListener(v -> navigate(R.id.action_endpointFragment_to_transfersFragment));

    getActivity().setTitle(viewModel.toString());
    return binding.getRoot();
  }

  private void navigate(int resId) {
    NavHostFragment navHostFragment =
            (NavHostFragment) ((FragmentActivity) getContext()).getSupportFragmentManager().
                    findFragmentById(R.id.fragmentContainerView);
    NavController navController = navHostFragment.getNavController();
    Bundle bundle = new Bundle();
    bundle.putString("id", viewModel.id);
    navController.navigate(resId, bundle);
  }
}