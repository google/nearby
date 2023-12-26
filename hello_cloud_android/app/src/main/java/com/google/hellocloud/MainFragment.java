package com.google.hellocloud;

import static com.google.hellocloud.Util.TAG;

import android.content.ContentResolver;
import android.content.Context;
import android.content.ContextWrapper;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.PickVisualMediaRequest;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.databinding.BindingAdapter;
import androidx.databinding.DataBindingUtil;
import androidx.fragment.app.Fragment;
import com.google.hellocloud.databinding.FragmentMainBinding;
import com.google.hellocloud.databinding.ItemEndpointBinding;
import java.util.ArrayList;
import java.util.List;

/** Fragment for the home screen */
public class MainFragment extends Fragment {
  static class EndpointAdapter extends ArrayAdapter<Endpoint> {
    private final Context context;

    public EndpointAdapter(Context context, List<Endpoint> endpoints) {
      super(context, R.layout.item_endpoint, endpoints);
      this.context = context;
    }

    @Override
    public @NonNull View getView(int position, View convertView, ViewGroup parent) {
      ItemEndpointBinding binding;
      View view;

      if (convertView == null) {
        LayoutInflater inflater = LayoutInflater.from(context);
        binding = DataBindingUtil.inflate(inflater, R.layout.item_endpoint, parent, false);
        view = binding.getRoot();
      } else {
        binding = DataBindingUtil.getBinding(convertView);
        view = convertView;
      }
      final Endpoint endpoint = getItem(position);
      binding.setModel(endpoint);

      binding.connect.setOnClickListener(
          v -> {
            endpoint.setState(Endpoint.State.CONNECTING);
            Main.shared.requestConnection(endpoint.id);
          });

      binding.disconnect.setOnClickListener(
          v -> {
            endpoint.setState(Endpoint.State.DISCONNECTING);
            Main.shared.disconnect(endpoint.id);
          });

      binding.pick.setOnClickListener(
          v -> {
            MainFragment mainFragment = getMainFragment();
            mainFragment.pickMedia(endpoint);
          });

      return view;
    }

    // TODO: this is super ugly, lol. But I have no time to think this through right now.
    private MainFragment getMainFragment() {
      Context context = getContext();
      while (context instanceof ContextWrapper) {
        if (context instanceof MainActivity mainActivity) {
          Fragment navHostFragment =
              mainActivity.getSupportFragmentManager().findFragmentById(R.id.fragmentContainerView);
          Fragment mainFragment =
              navHostFragment == null
                  ? null
                  : navHostFragment.getChildFragmentManager().getFragments().get(0);
          return (MainFragment) mainFragment;
        }
        context = ((ContextWrapper) context).getBaseContext();
      }
      return null;
    }
  }

  private final Main model = Main.shared;
  private Endpoint endpointForPicker = null;

  ActivityResultLauncher<PickVisualMediaRequest> picker;

  @Override
  public View onCreateView(
      @NonNull LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    picker =
        registerForActivityResult(
            new ActivityResultContracts.PickMultipleVisualMedia(), this::onMediaPicked);

    FragmentMainBinding binding =
        DataBindingUtil.inflate(inflater, R.layout.fragment_main, container, false);
    Main.shared.context = getActivity().getApplicationContext();
    binding.setModel(model);
    getActivity().setTitle(R.string.app_name);
    return binding.getRoot();
  }

  public void pickMedia(Endpoint endpoint) {
    assert picker != null;
    endpointForPicker = endpoint;
    picker.launch(
        new PickVisualMediaRequest.Builder()
            .setMediaType(ActivityResultContracts.PickVisualMedia.ImageAndVideo.INSTANCE)
            .build());
  }

  private void onMediaPicked(List<Uri> uris) {
    if (uris.size() == 0) {
      return;
    }

    if (endpointForPicker == null) {
      Log.e(TAG, "onMediaPicked() called without setting endpointForPicker");
      return;
    }

    Context context = getView().getContext();
    new AlertDialog.Builder(context)
        .setMessage("Do you want to send the claim token to the remote endpoint?")
        .setPositiveButton(
            "Yes",
            (dialog, button) -> {
              endpointForPicker.sendPacket(context, uris);
            })
        .setNegativeButton("No", null)
        .show();
  }

  @BindingAdapter("entries")
  public static void setEntries(View view, List<Endpoint> endpoints) {
    Context context = view.getContext();
    ArrayAdapter<Endpoint> endpointAdapter = new EndpointAdapter(context, endpoints);
    ((ListView) view).setAdapter(endpointAdapter);
  }
}
