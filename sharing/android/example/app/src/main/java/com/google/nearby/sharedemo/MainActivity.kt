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
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.slice.Slice
import androidx.slice.widget.SliceView
import com.google.nearby.sharedemo.ui.theme.NearbyShareDemoTheme

class MainActivity : ComponentActivity() {

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    val viewModel: MainViewModel by viewModels(factoryProducer = { MainViewModel.Factory })
    setContent {
      NearbyShareDemoTheme {
        // A surface container using the 'background' color from the theme
        val state by viewModel.targetsFlow.collectAsState()
        Column(
          modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
        ) {
          MainView(
            state,
            onShareTargetClicked = { data, intent ->
              viewModel.onShareTargetClicked(data, this@MainActivity, intent)
            })
        }
      }
    }
  }
}

@Composable
fun MainView(
  state: SliceState,
  onShareTargetClicked: (ShareTargetData, Intent) -> Unit,
) {
  when (state) {
    is SliceState.PermissionNeeded -> {
      AndroidView(factory = {
        val view = SliceView(it)
        view.slice = state.slice
        return@AndroidView view
      })
    }

    is SliceState.Active -> {
      var uris by rememberSaveable { mutableStateOf(listOf<Uri>()) }
      val launcher =
        rememberLauncherForActivityResult(contract = ActivityResultContracts.GetMultipleContents()) {
          uris = it
        }
      Column {
        Button(onClick = { launcher.launch("*/*") }) { Text("Select files") }
        if (uris.isNotEmpty()) {
          for (uri in uris) {
            Text(
              uri.toString(),
              style = MaterialTheme.typography.bodySmall
            )
          }
          val sendIntent =
            Intent("com.google.android.gms.SHARE_NEARBY").apply {
              if (uris.size == 1) {
                putExtra(Intent.EXTRA_STREAM, uris[0])
              } else {
                putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris.toArrayList())
              }
              type = "*/*"
              flags = Intent.FLAG_GRANT_READ_URI_PERMISSION
            }
          val context = LocalContext.current
          // We need to call sendIntent.migrateExtraStreamToClipData() to show the preview images on
          // Android Q and above but the method is hidden. Hence we call PendingIntent.getActivity as
          // a proxy/wrapper which calls the above method. This method can throw exception if files
          // are too large.
          // We need to call sendIntent.migrateExtraStreamToClipData() to show the preview images on
          // Android Q and above but the method is hidden. Hence we call PendingIntent.getActivity as
          // a proxy/wrapper which calls the above method. This method can throw exception if files
          // are too large.
          val pendingIntentFlags = PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT

          PendingIntent.getActivity(context.applicationContext, 0, sendIntent, pendingIntentFlags)
          ShareDestinations(sendIntent, state.targets, onShareTargetClicked = { data, intent ->
            for (uri in uris) {
              context.grantUriPermission(
                "com.google.android.gms",
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
              )
            }
            onShareTargetClicked(data, intent)
          })
        }
      }
    }
  }
}

@Composable
fun ShareDestinations(
  sendIntent: Intent,
  targets: Set<ShareTargetData>,
  onShareTargetClicked: (ShareTargetData, Intent) -> Unit,
) {
  Card(
    modifier = Modifier.padding(16.dp),
    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
  ) {
    if (targets.isEmpty()) {
      Text("No devices nearby!")
      return@Card
    }
    LazyVerticalGrid(columns = GridCells.Adaptive(minSize = 72.dp)) {
      items(targets.toList()) {
        ShareTarget(data = it, onShareTargetClicked = { data ->
          onShareTargetClicked(data, sendIntent)
        })
      }
    }
  }
}

private fun <T> List<T>.toArrayList(): java.util.ArrayList<T> {
  val list = java.util.ArrayList<T>()
  list.addAll(this)
  return list
}

sealed class SliceState {
  data class PermissionNeeded(val slice: Slice) : SliceState()
  data class Active(val targets: Set<ShareTargetData>) : SliceState()
}
