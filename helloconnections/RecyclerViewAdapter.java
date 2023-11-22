package com.google.location.nearby.apps.helloconnections;

import android.graphics.Color;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import com.google.location.nearby.apps.helloconnections.RecyclerViewAdapter.ItemViewHolder;
import java.util.ArrayList;
import java.util.Collections;

class RecyclerViewAdapter extends RecyclerView.Adapter<ItemViewHolder>
    implements ItemMoveCallback.ItemTouchHelperContract {

  private final ArrayList<String> data;

  public static class ItemViewHolder extends RecyclerView.ViewHolder {

    private final TextView title;
    View rowView;

    public ItemViewHolder(View itemView) {
      super(itemView);

      rowView = itemView;
      title = itemView.findViewById(R.id.title);
    }
  }

  public RecyclerViewAdapter(ArrayList<String> data) {
    this.data = data;
  }

  @Override
  public ItemViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
    View itemView =
        LayoutInflater.from(parent.getContext()).inflate(R.layout.cardview_row, parent, false);
    return new ItemViewHolder(itemView);
  }

  @Override
  public void onBindViewHolder(ItemViewHolder holder, int position) {
    holder.title.setText(data.get(position));
  }

  @Override
  public int getItemCount() {
    return data.size();
  }

  @Override
  public void onRowMoved(int fromPosition, int toPosition) {
    if (fromPosition < toPosition) {
      for (int i = fromPosition; i < toPosition; i++) {
        Collections.swap(data, i, i + 1);
      }
    } else {
      for (int i = fromPosition; i > toPosition; i--) {
        Collections.swap(data, i, i - 1);
      }
    }
    notifyItemMoved(fromPosition, toPosition);
  }

  @Override
  public void onRowDismissed(int position) {
    data.remove(position);
    notifyItemRemoved(position);
  }

  @Override
  public void onRowSelected(ItemViewHolder itemViewHolder) {
    itemViewHolder.rowView.setBackgroundColor(Color.GRAY);
  }

  @Override
  public void onRowClear(ItemViewHolder itemViewHolder) {
    itemViewHolder.rowView.setBackgroundColor(Color.WHITE);
  }

  public ArrayList<String> getData() {
    return data;
  }
}
