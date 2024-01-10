# Nearby Sharing Android API

## The slice API
The nearby module in GMSCore provides a third-party accessible slice to bind to, to provide live data on nearby targets using Nearby Share. Clicking on any of these targets will take you to the main Nearby Share screen, where the transfer will begin and continually update the progress.

The slice URI is `content://com.google.android.gms.nearby.sharing/scan`. Upon first launch after install, you will be prompted to grant permission to the client application. After that, the slice will be kept up to date with the latest targets nearby.
