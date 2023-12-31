package com.google.hellocloud;

import static com.google.hellocloud.Utils.TAG;
import static com.google.hellocloud.Utils.requestPermissionsIfNeeded;

import android.Manifest;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.databinding.DataBindingUtil;
import androidx.fragment.app.Fragment;
import androidx.navigation.NavController;
import androidx.navigation.fragment.NavHostFragment;
import com.google.firebase.messaging.FirebaseMessaging;
import com.google.hellocloud.databinding.ItemIncomingFileBinding;
import com.google.hellocloud.databinding.ItemIncomingPacketBinding;
import com.google.hellocloud.databinding.ItemOutgoingPacketBinding;
import com.google.zxing.integration.android.IntentIntegrator;
import com.google.zxing.integration.android.IntentResult;
import java.util.UUID;

public class MainActivity extends AppCompatActivity {
  private static final String[] REQUIRED_PERMISSIONS_FOR_APP =
      new String[] {
        Manifest.permission.READ_CONTACTS,
        Manifest.permission.BLUETOOTH_SCAN,
        Manifest.permission.BLUETOOTH_ADVERTISE,
        Manifest.permission.BLUETOOTH_CONNECT,
        Manifest.permission.ACCESS_WIFI_STATE,
        Manifest.permission.CHANGE_WIFI_STATE,
        Manifest.permission.NEARBY_WIFI_DEVICES,
        Manifest.permission.POST_NOTIFICATIONS,
      };

  private static final int REQUIRE_PERMISSIONS_FOR_APP_REQUEST_CODE = 0;

  public void showBackButton() {
    ActionBar actionBar = getSupportActionBar();
    if (actionBar != null) {
      actionBar.setDisplayHomeAsUpEnabled(true);
    }
  }

  private void hideBackButton() {
    ActionBar actionBar = getSupportActionBar();
    if (actionBar != null) {
      actionBar.setDisplayHomeAsUpEnabled(false);
    }
  }

  private void createNotificationChannel() {
    CharSequence name = "HelloCloud";
    String description = "Notification channel for receiving packet status updates";
    int importance = NotificationManager.IMPORTANCE_HIGH;
    NotificationChannel channel = new NotificationChannel("DEFAULT_CHANNEL", name, importance);
    channel.setDescription(description);
    // Register the channel with the system; you can't change the importance
    // or other notification behaviors after this.
    NotificationManager notificationManager = getSystemService(NotificationManager.class);
    notificationManager.createNotificationChannel(channel);
  }

  @Override
  protected void onNewIntent(Intent intent) {
    super.onNewIntent(intent);

    // If we were launched as a result of tapping a notification, go to inbox and flash the packet
    Bundle bundle = intent.getExtras();
    if (bundle != null) {
      String packetId = bundle.getString("packetId");
      if (packetId != null) {
        showIncomingPackets(null);
        Main.shared.onPacketUploaded(UUID.fromString(packetId));
      }
    }
  }

  @Override
  protected void onResume() {
    super.onResume();

    // If we were launched as a result of tapping a notification, go to inbox and flash the packet
    Intent intent = getIntent();
    Bundle bundle = intent.getExtras();
    if (bundle != null) {
      String packetId = bundle.getString("packetId");
      if (packetId != null) {
        showIncomingPackets(null);
        Main.shared.onPacketUploaded(UUID.fromString(packetId));
      }
    }
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    requestPermissionsIfNeeded(
        this, REQUIRED_PERMISSIONS_FOR_APP, REQUIRE_PERMISSIONS_FOR_APP_REQUEST_CODE);

    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    hideBackButton();

    createNotificationChannel();
    FirebaseMessaging.getInstance()
        .getToken()
        .addOnCompleteListener(
            task -> {
              if (!task.isSuccessful()) {
                Log.w(TAG, "Fetching FCM registration token failed", task.getException());
                return;
              }
              Main.shared.notificationToken = task.getResult();
              Log.i(TAG, "FCM notification token: " + task.getResult());
            });
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
    super.onActivityResult(requestCode, resultCode, data);

    IntentResult intentResult = IntentIntegrator.parseActivityResult(requestCode, resultCode, data);
    if (intentResult == null) {
      return;
    }

    String content = intentResult.getContents();
    if (content == null) {
      return;
    }

    Fragment navHostFragment =
        getSupportFragmentManager().findFragmentById(R.id.fragmentContainerView);
    if (navHostFragment == null) {
      return;
    }

    MainFragment mainFragment =
        (MainFragment) navHostFragment.getChildFragmentManager().getFragments().get(0);
    if (mainFragment == null) {
      return;
    }

    mainFragment.onQrCodeReceived(content);
  }

  @Override
  public boolean onOptionsItemSelected(@NonNull MenuItem item) {
    // Handle action bar "back" button
    if (item.getItemId() == android.R.id.home) {
      getOnBackPressedDispatcher().onBackPressed();
      return true;
    }
    return super.onOptionsItemSelected(item);
  }

  public void showIncomingPackets(View view) {
    NavHostFragment navHostFragment =
        (NavHostFragment)
            this.getSupportFragmentManager().findFragmentById(R.id.fragmentContainerView);
    NavController navController = navHostFragment.getNavController();
    Bundle bundle = new Bundle();
    navController.navigate(R.id.incomingPacketsFragment, bundle);
  }

  public void showOutgoingPackets(View view) {
    NavHostFragment navHostFragment =
        (NavHostFragment)
            this.getSupportFragmentManager().findFragmentById(R.id.fragmentContainerView);
    NavController navController = navHostFragment.getNavController();
    Bundle bundle = new Bundle();
    navController.navigate(R.id.action_mainFragment_to_outgoingPacketsFragment, bundle);
  }

  private static View getParentView(View view) {
    return (View) view.getParent();
  }

  public void showIncomingFile(View view) {
    ItemIncomingFileBinding binding;

    View fileView = getParentView(view);
    while (fileView != null) {
      binding = DataBindingUtil.getBinding(fileView);
      if (binding != null) {
        Uri imageUri = binding.getModel().getLocalUri();

        new ImageViewDialogFragment(imageUri).show(getSupportFragmentManager(), TAG);

        break;
      }
      fileView = getParentView(fileView);
    }
    Log.e(TAG, "Incoming file clicked. But file view was not found.");
  }

  public void uploadPacket(View view) {
    ItemOutgoingPacketBinding binding;
    View packetView = getParentView(view);
    while (packetView != null) {
      binding = DataBindingUtil.getBinding(packetView);
      if (binding != null) {
        binding.getModel().upload();
        break;
      }
      packetView = getParentView(packetView);
    }
    Log.e(TAG, "Upload button clicked. But packet view was not found.");
  }

  public void downloadPacket(View view) {
    ItemIncomingPacketBinding binding;
    View packetView = getParentView(view);
    while (packetView != null) {
      binding = DataBindingUtil.getBinding(packetView);
      if (binding != null) {
        binding.getModel().download(view.getContext());
        break;
      }
      packetView = getParentView(packetView);
    }
    Log.e(TAG, "Download button clicked. But packet view was not found.");
  }
}
