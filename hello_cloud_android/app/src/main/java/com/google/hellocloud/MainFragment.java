package com.google.hellocloud;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import androidx.annotation.NonNull;
import androidx.databinding.BindingAdapter;
import androidx.databinding.DataBindingUtil;
import androidx.fragment.app.Fragment;

import com.google.hellocloud.databinding.FragmentMainBinding;
import com.google.hellocloud.databinding.ItemEndpointBinding;

import java.util.List;

/**
 * Fragment for the home screen
 */
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

      if (convertView == null)
      {
        LayoutInflater inflater = LayoutInflater.from(context);
        binding = DataBindingUtil.inflate(inflater, R.layout.item_endpoint, parent, false);
        view = binding.getRoot();
      } else {
        binding = DataBindingUtil.getBinding(convertView);
        view = convertView;
      }
      final Endpoint viewModel = getItem(position);
      binding.setModel(viewModel);

      //      view.setOnClickListener(v -> {
      //        NavHostFragment navHostFragment =
      //                (NavHostFragment) ((FragmentActivity)
      // this.context).getSupportFragmentManager().
      //                        findFragmentById(R.id.fragmentContainerView);
      //        NavController navController = navHostFragment.getNavController();
      //        Bundle bundle = new Bundle();
      //        bundle.putString("id", viewModel.id);
      //        navController.navigate(R.id.action_homeFragment_to_endpointFragment, bundle);
      //      });
      return view;
    }
  }

  private Main model = Main.shared;

  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
                           Bundle savedInstanceState) {
    // TODO: fill id and name here
    FragmentMainBinding binding = DataBindingUtil.inflate(inflater, R.layout.fragment_main, container, false);
    Main.shared.context = getActivity().getApplicationContext();
    binding.setModel(model);
    getActivity().setTitle(R.string.app_name);
    return binding.getRoot();
  }

  @BindingAdapter("entries")
  public static void setEntries(View view, List<Endpoint> endpoints) {
    Context context = view.getContext();
    ArrayAdapter<Endpoint> endpointAdapter = new EndpointAdapter(context, endpoints);
    ((ListView) view).setAdapter(endpointAdapter);
  }
}