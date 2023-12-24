package com.google.hellocloud;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import androidx.annotation.NonNull;
import androidx.databinding.DataBindingUtil;
import androidx.databinding.ViewDataBinding;
import androidx.fragment.app.Fragment;

import java.util.List;
import java.util.Optional;

public class ListOnEndpointFragment extends Fragment {
  protected Endpoint model;
  protected final int resourceFragment;

  public ListOnEndpointFragment(int resourceFragment) {
    this.resourceFragment = resourceFragment;
  }

  static class SimpleArrayAdapter<T> extends ArrayAdapter<T> {
    private final Context context;
    private final int resource;
    private final int variableId;

    public SimpleArrayAdapter(int resource, int variableId, Context context, List<T> items) {
      super(context, resource, items);
      this.context = context;
      this.resource = resource;
      this.variableId = variableId;
    }

    @Override
    public @NonNull View getView(int position, View convertView, ViewGroup parent) {
      ViewDataBinding binding;
      View view;

      if (convertView == null) {
        LayoutInflater inflater = LayoutInflater.from(context);
        binding = DataBindingUtil.inflate(inflater, resource, parent, false);
        view = binding.getRoot();
      } else {
        binding = DataBindingUtil.getBinding(convertView);
        view = convertView;
      }

      final T item = getItem(position);
      assert binding != null;
      binding.setVariable(variableId, item);
      return view;
    }
  }

  @Override
  public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
                           Bundle savedInstanceState) {
    ViewDataBinding binding = DataBindingUtil.inflate(
            inflater, resourceFragment, container, false);

    assert getArguments() != null;
    String id = getArguments().getString("id");
    Optional<Endpoint> maybeEndpoint = Main.shared.getEndpoint(id);
    if (maybeEndpoint.isPresent()) {
      model = maybeEndpoint.get();
      binding.setVariable(BR.model, model);
      assert getActivity() != null;
      getActivity().setTitle(maybeEndpoint.get().toString());
      return binding.getRoot();
    } else {
      throw new RuntimeException("Endpoint not found in ListOnEndpointFragment.onCreateView()");
    }
  }

  public static <T> void setEntries(int resourceItem, int variableId, View view, List<T> files) {
    Context context = view.getContext();
    SimpleArrayAdapter<T> adapter = new SimpleArrayAdapter<>(
            resourceItem, variableId, context, files);
    ((ListView) view).setAdapter(adapter);
  }
}
