# Nearby Sharing Android API

## Checking if Quick Share (Google) is supported on your device

Quick Share by Google has its own UI, and to use the below launch intents, the availability of these intents needs to be checked first.
This can be done by calling `PackageManager#queryIntentActivities()` with the intent required. A good rule of thumb to check if Quick Share is enabled is to run this check with the intent `com.google.android.gms.SHARE_NEARBY`.

Example code:

```kotlin
val shareIntent = Intent("com.google.android.gms.SHARE_NEARBY")
        .setType("text/plain")
        .putExtra(Intent.EXTRA_TEXT, "Hello Nearby");

val packageManager = context.packageManager;
val activities = packageManager.queryIntentActivities(intent, PackageManager.MATCH_DEFAULT_ONLY);
// If the activities list is not empty, the intent can be handled and the intent can be called!
```

## Launching the settings page

The Google Quick Share settings page can be launched by sending an intent with the action `com.google.android.gms.settings.SHARING`. There are no extras to be provided.

Example code:

```kotlin
const val QUICK_SHARE_SETTINGS_INTENT = "com.google.android.gms.settings.SHARING"
context.startActivity(Intent(QUICK_SHARE_SETTINGS_INTENT))
```

## Launching the receiving UI

The Google Quick Share receiving UI can be launched by sending an intent with the action `com.google.android.gms.RECEIVE_NEARBY`. You will need to set the type of the intent to be `*/*` to match the filter.

Example code:

```kotlin
const val QUICK_SHARE_RECEIVE_INTENT = "com.google.android.gms.RECEIVE_NEARBY"
context.startActivity(Intent(QUICK_SHARE_RECEIVE_INTENT).setType("*/*"))
```

## Sending a file via Quick Share

Quick Share responds to the `android.intent.action.SEND` intent. To send a file, attach a content URI to the Intent.EXTRA_STREAM when sending the intent to send a file via Quick Share.

Example code:

```kotlin
val sendIntent = Intent(Intent.ACTION_SEND)
        .setType(/* Set your content mime type here */)
        .putExtra(Intent.EXTRA_STREAM, /* your content URI here */)
```

## Sending a folder via Quick Share

To send folders, Quick Share has a custom intent: `com.google.android.gms.nearby.SEND_FOLDER`.

### Mandatory extras

To place files in a parent folder, you can supply the following string extra: `com.google.android.gms.nearby.PARENT_FOLDER`. If this is not desirable, leave it as an empty string.

To provide the file count of the number of the files in the folder, supply the following extra: `com.google.android.gms.nearby.FILE_COUNT`. If this is not supplied, Quick Share will assume 0 files are in the folder.

To provide the actual content of the folder, supply the following extra: `com.google.android.gms.nearby.SEND_FOLDER_CONTENT_URI`. If not available, Quick Share will fail the transfer.

### Content provider

To share a folder, Quick Share expects the presence of a ContentProvider with the following columns:

* `file_name` - The name of the file.
* `file_uri` - The URI of the file, as given by a file provider.
* `file_size` - The size in bytes of the file.
* `file_dir` - The directory that the file should be placed in (parent directory under `/sdcard/Download/Quick Share`).
* `file_mime_type` - The mime type of the file.

Quick Share expects these columns to be populated for each file the user wants to share. The content URI provided is the URI to the content provider that can be queried for each file.

Putting this all together, we have the following intent structure:

```kotlin
val sendFolderIntent = Intent("com.google.android.gms.nearby.SEND_FOLDER")
        .putExtra("com.google.android.gms.nearby.PARENT_FOLDER", /* The folder to save to under /sdcard/Download/Quick Share */)
        .putExtra("com.google.android.gms.nearby.FILE_COUNT", /* The file count in the folder */)
        .putExtra("com.google.android.gms.nearby.SEND_FOLDER_CONTENT_URI", /* Your content provider URI here */)
```


## Slice API

The nearby module in GMSCore provides a third-party accessible slice to bind to, to provide live data on nearby targets using Quick Share. Clicking on any of these targets will take you to the main Quick Share screen, where the transfer will begin and continually update the progress.

The slice URI is `content://com.google.android.gms.nearby.sharing/scan`. Upon first launch after install, you will be prompted to grant permission to the client application. After that, the slice will be kept up to date with the latest targets nearby.

### How to use the API

Example code:

```kotlin
// Derive the Slice URI.
val sliceUri = Uri.parse("content://com.google.android.gms.nearby.sharing/scan")
// Get the SliceViewManager
val sliceManager = SliceViewManager.getInstance(context)
// Pin the slice and register your callback.
sliceManager.registerSliceCallback(sliceUri, { slice: Slice? ->
    if (slice == null) {
        return
    }
    for (targetItem in slice.items.reversed()) {
      // Each row containing a target has the hints LIST_ITEM and ACTIVITY.
      if (!(targetItem.format == SLICE && targetItem.hints.containsAll(listOf(LIST_ITEM, ACTIVITY)))) {
        continue
      }
      val targetSlice = targetItem.slice
      var deviceName: String? = null
      var action: PendingIntent? = null
      var profileIcon: IconCompat? = null

      for (item in targetSlice.items) {
        // The slice item of the target's device name contains the TITLE hint.
        if (item.format == TEXT && item.hints.contains(TITLE)) {
          deviceName = item.text.toString()
        }
        // The slice item of the target action contains the SHORTCUT and TITLE hints.
        if (item.format == ACTION && item.hints.containsAll(listOf(SHORTCUT, TITLE))) {
          action = item.action

          val iconSlice: Slice? = item.slice
          if (iconSlice != null) {
            for (iconitem in iconSlice.items) {
              // The target's icon is indicated by the IMAGE slice item format and the NO_TINT hint.
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
    }
})
// Remember to bind the slice after you pin it to avoid race conditions!
val slice = sliceManager.bindSlice(sliceUri)
```
Refer to the [SliceViewManager#registerSliceCallback() documentation](https://developer.android.com/reference/androidx/slice/SliceViewManager#registerSliceCallback(android.net.Uri,androidx.slice.SliceViewManager.SliceCallback)) for more information about the race condition described above.
