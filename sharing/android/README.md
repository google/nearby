# Nearby Sharing Android API

## The slice API
The nearby module in GMSCore provides a third-party accessible slice to bind to, to provide live data on nearby targets using Nearby Share. Clicking on any of these targets will take you to the main Nearby Share screen, where the transfer will begin and continually update the progress.

The slice URI is `content://com.google.android.gms.nearby.sharing/scan`. Upon first launch after install, you will be prompted to grant permission to the client application. After that, the slice will be kept up to date with the latest targets nearby.

## How to use the API ##
In Kotlin:
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
