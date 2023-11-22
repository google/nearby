package com.google.location.nearby.apps.helloconnections;

import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;
import androidx.annotation.NonNull;
import com.google.location.nearby.apps.helloconnections.RecyclerViewAdapter.ItemViewHolder;

class ItemMoveCallback extends ItemTouchHelper.Callback {

  private final ItemTouchHelperContract adapter;

  public ItemMoveCallback(ItemTouchHelperContract adapter) {
    this.adapter = adapter;
  }

  @Override
  public boolean isLongPressDragEnabled() {
    return true;
  }

  @Override
  public boolean isItemViewSwipeEnabled() {
    return true;
  }

  @Override
  public void onSwiped(@NonNull RecyclerView.ViewHolder viewHolder, int direction) {
    adapter.onRowDismissed(viewHolder.getAdapterPosition());
  }

  @Override
  public int getMovementFlags(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
    int dragFlags = ItemTouchHelper.UP | ItemTouchHelper.DOWN;
    int swipeFlags = ItemTouchHelper.START | ItemTouchHelper.END;
    return makeMovementFlags(dragFlags, swipeFlags);
  }

  @Override
  public boolean onMove(
      RecyclerView recyclerView,
      RecyclerView.ViewHolder viewHolder,
      RecyclerView.ViewHolder target) {
    adapter.onRowMoved(viewHolder.getAdapterPosition(), target.getAdapterPosition());
    return true;
  }

  @Override
  public void onSelectedChanged(RecyclerView.ViewHolder viewHolder, int actionState) {
    if (actionState != ItemTouchHelper.ACTION_STATE_IDLE) {
      if (viewHolder instanceof ItemViewHolder) {
        ItemViewHolder itemViewHolder = (ItemViewHolder) viewHolder;
        adapter.onRowSelected(itemViewHolder);
      }
    }

    super.onSelectedChanged(viewHolder, actionState);
  }

  @Override
  public void clearView(RecyclerView recyclerView, RecyclerView.ViewHolder viewHolder) {
    super.clearView(recyclerView, viewHolder);

    if (viewHolder instanceof ItemViewHolder) {
      ItemViewHolder itemViewHolder = (ItemViewHolder) viewHolder;
      adapter.onRowClear(itemViewHolder);
    }
  }

  public interface ItemTouchHelperContract {

    void onRowMoved(int fromPosition, int toPosition);

    void onRowDismissed(int position);

    void onRowSelected(ItemViewHolder itemViewHolder);

    void onRowClear(ItemViewHolder itemViewHolder);
  }
}
