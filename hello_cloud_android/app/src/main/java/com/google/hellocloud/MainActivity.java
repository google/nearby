package com.google.hellocloud;

import static com.google.hellocloud.Utils.TAG;
import static com.google.hellocloud.Utils.requestPermissionsIfNeeded;

import android.Manifest;
import android.os.Bundle;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import androidx.annotation.NonNull;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.databinding.DataBindingUtil;
import androidx.fragment.app.FragmentActivity;
import androidx.navigation.NavController;
import androidx.navigation.fragment.NavHostFragment;
import com.google.hellocloud.databinding.ItemIncomingPacketBinding;
import com.google.hellocloud.databinding.ItemOutgoingPacketBinding;

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
      };

  private static final int REQUIRE_PERMISSIONS_FOR_APP_REQUEST_CODE = 0;

  public void showBackButton() {
    ActionBar actionBar = getSupportActionBar();
    if (actionBar != null) {
      actionBar.setDisplayHomeAsUpEnabled(true);
    }
  }

  public void hideBackButton() {
    ActionBar actionBar = getSupportActionBar();
    if (actionBar != null) {
      actionBar.setDisplayHomeAsUpEnabled(false);
    }
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    requestPermissionsIfNeeded(
        this, REQUIRED_PERMISSIONS_FOR_APP, REQUIRE_PERMISSIONS_FOR_APP_REQUEST_CODE);

    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);
    hideBackButton();
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
            ((FragmentActivity) this)
                .getSupportFragmentManager()
                .findFragmentById(R.id.fragmentContainerView);
    NavController navController = navHostFragment.getNavController();
    Bundle bundle = new Bundle();
    navController.navigate(R.id.action_mainFragment_to_incomingPacketsFragment, bundle);
  }

  public void showOutgoingPackets(View view) {
    NavHostFragment navHostFragment =
        (NavHostFragment)
            ((FragmentActivity) this)
                .getSupportFragmentManager()
                .findFragmentById(R.id.fragmentContainerView);
    NavController navController = navHostFragment.getNavController();
    Bundle bundle = new Bundle();
    navController.navigate(R.id.action_mainFragment_to_outgoingPacketsFragment, bundle);
  }

  private static View getParentView(View view) {
    return (View) view.getParent();
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
