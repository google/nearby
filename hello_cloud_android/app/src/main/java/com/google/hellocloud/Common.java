package com.google.hellocloud;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseExpandableListAdapter;

import androidx.databinding.DataBindingUtil;
import androidx.databinding.ViewDataBinding;

import java.util.List;

class PacketListAdapter<T extends File> extends BaseExpandableListAdapter {
  private final Context context;
  private final List<Packet<T>> packets;

  private final int packetLayoutId;
  private  final int fileLayoutId;

  public PacketListAdapter(
          Context context, List<Packet<T>> packets, int packetLayoutId, int fileLayoutId) {
    this.context = context;
    this.packets = packets;
    this.packetLayoutId = packetLayoutId;
    this.fileLayoutId = fileLayoutId;
  }

  @Override
  public int getGroupCount() {
    return this.packets.size();
  }

  @Override
  public int getChildrenCount(int groupPosition) {
    return this.packets.get(groupPosition).files.size();
  }

  @Override
  public Object getGroup(int groupPosition) {
    return this.packets.get(groupPosition);
  }

  @Override
  public Object getChild(int groupPosition, int childPosition) {
    return this.packets.get(groupPosition).files.get(childPosition);
  }

  @Override
  public long getGroupId(int groupPosition) {
    // TODO: refactor packet ID to be 64 bits and use it here
    return groupPosition;
  }

  @Override
  public long getChildId(int groupPosition, int childPosition) {
    // TODO: refactor file ID to be 64 bits and use it here
    return childPosition;
  }

  @Override
  public View getGroupView(
          int groupPosition, boolean isExpanded, View convertView, ViewGroup parent) {
    View view;
    ViewDataBinding binding;
    if (convertView == null) {
      LayoutInflater inflater = LayoutInflater.from(context);
      binding = DataBindingUtil.inflate(inflater, packetLayoutId, parent, false);
      view = binding.getRoot();
    } else {
      binding = DataBindingUtil.getBinding(convertView);
      view = convertView;
    }
    final Packet<T> packet = (Packet<T>) getGroup(groupPosition);
    binding.setVariable(BR.model, packet);
    return view;
  }

  @Override
  public View getChildView(
          int groupPosition,
          final int childPosition,
          boolean isLastChild,
          View convertView,
          ViewGroup parent) {
    View view;
    ViewDataBinding binding;
    if (convertView == null) {
      LayoutInflater inflater = LayoutInflater.from(context);
      binding = DataBindingUtil.inflate(inflater, fileLayoutId, parent, false);
      view = binding.getRoot();
    } else {
      binding = DataBindingUtil.getBinding(convertView);
      view = convertView;
    }
    final T file = (T) getChild(groupPosition, childPosition);
    binding.setVariable(BR.model, file);
    return view;
  }

  @Override
  public boolean hasStableIds() {
    return true;
  }

  @Override
  public boolean isChildSelectable(int listPosition, int expandedListPosition) {
    return false;
  }
}
