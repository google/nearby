/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.nearby.sharedemo

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.core.graphics.drawable.IconCompat
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewmodel.initializer
import androidx.lifecycle.viewmodel.viewModelFactory
import androidx.slice.Slice
import androidx.slice.SliceViewManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class MainViewModel(context: Context) : ViewModel() {
  private val _targetsFlow: MutableStateFlow<SliceState> = MutableStateFlow(SliceState.Active(setOf()))
  val targetsFlow: StateFlow<SliceState> = _targetsFlow.asStateFlow()

  private var targetMapping = mapOf<ShareTargetData, PendingIntent>()

  private val sliceManager = SliceViewManager.getInstance(context)
  private val sliceCallback: (Slice?) -> Unit = {
    targetMapping = parseSlice(it)
    if (targetMapping.isEmpty() && it != null) {
      _targetsFlow.value = SliceState.PermissionNeeded(it)
    } else {
      _targetsFlow.value = SliceState.Active(targetMapping.keys)
    }
  }

  init {
    sliceManager.registerSliceCallback(SCAN_SLICE_URI, sliceCallback)
    val slice = sliceManager.bindSlice(SCAN_SLICE_URI)
    targetMapping = parseSlice(slice)
    if (targetMapping.isEmpty() && slice != null) {
      _targetsFlow.value = SliceState.PermissionNeeded(slice)
    } else {
      _targetsFlow.value = SliceState.Active(targetMapping.keys)
    }
  }

  /**
   * This function is run just before the ViewModel is closed, allowing us to unpin the sharing
   * slice.
   */
  override fun onCleared() {
    super.onCleared()
    sliceManager.unregisterSliceCallback(SCAN_SLICE_URI, sliceCallback)
  }

  /**
   * Called when a slice's share target is tapped, as represented by [ShareTarget].
   */
  fun onShareTargetClicked(data: ShareTargetData, context: Context, sendIntent: Intent) {
    targetMapping[data]!!.send(context, 0, sendIntent)
  }

  private fun parseSlice(slice: Slice?): Map<ShareTargetData, PendingIntent> {
    android.util.Log.d("NSDemo", "slice: $slice")
    if (slice == null) {
      return mapOf()
    }
    val ret = mutableMapOf<ShareTargetData, PendingIntent>()
    for (targetItem in slice.items.reversed()) {
      if (!(targetItem.format == SLICE && targetItem.hints.containsAll(listOf(LIST_ITEM, ACTIVITY)))) {
        continue
      }
      val targetSlice = targetItem.slice
      var deviceName: String? = null
      var action: PendingIntent? = null
      var profileIcon: IconCompat? = null

      for (item in targetSlice.items) {
        if (item.format == TEXT && item.hints.contains(TITLE)) {
          deviceName = item.text.toString()
        }
        if (item.format == ACTION && item.hints.containsAll(listOf(SHORTCUT, TITLE))) {
          action = item.action

          val iconSlice: Slice? = item.slice
          if (iconSlice != null) {
            for (iconitem in iconSlice.items) {
              if (iconitem.format == IMAGE && iconitem.hints.contains(NO_TINT)) {
                profileIcon = iconitem.icon
              }
            }
          }
        }
      }
      // Returns null if the data parsed from the slice is incomplete.
      if (deviceName == null || action == null || profileIcon == null) {
        continue
      }
      ret[ShareTargetData(profileIcon, deviceName)] = action
    }
    return ret
  }

  companion object {
    private val SCAN_SLICE_URI: Uri =
      Uri.parse("content://com.google.android.gms.nearby.sharing/scan")

    // Slice parsing.
    private const val SLICE = "slice"
    private const val LIST_ITEM = "list_item"
    private const val ACTIVITY = "activity"
    private const val TEXT = "text"
    private const val TITLE = "title"
    private const val ACTION = "action"
    private const val SHORTCUT = "shortcut"
    private const val IMAGE = "image"
    private const val NO_TINT = "no_tint"

    val Factory: ViewModelProvider.Factory = viewModelFactory {
      initializer {
        MainViewModel(this[ViewModelProvider.AndroidViewModelFactory.APPLICATION_KEY]!!)
      }
    }
  }
}
