package com.google.nearby.sharedemo

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.graphics.drawable.IconCompat
import androidx.core.graphics.drawable.toBitmap

@Composable
fun ShareTarget(data: ShareTargetData, onShareTargetClicked: (ShareTargetData) -> Unit) {
  val context = LocalContext.current
  Column(
    modifier = Modifier
      .padding(8.dp)
      .clickable { onShareTargetClicked(data) },
    horizontalAlignment = Alignment.CenterHorizontally
  ) {
    Icon(
      data.profileIcon.loadDrawable(context)!!.toBitmap().asImageBitmap(),
      contentDescription = null,
      tint = Color.Unspecified,
    )
    Text(
      data.deviceName,
      style = MaterialTheme.typography.bodySmall,
    )
  }
}

data class ShareTargetData(
  val profileIcon: IconCompat,
  val deviceName: String,
)
