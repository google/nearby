package com.google.hellocloud;

import static com.google.hellocloud.Util.requestPermissionsIfNeeded;

import android.Manifest;
import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
  private static final String[] REQUIRED_PERMISSIONS_FOR_APP =
          new String[]{
                  Manifest.permission.READ_CONTACTS,
                  Manifest.permission.BLUETOOTH_SCAN,
                  Manifest.permission.BLUETOOTH_ADVERTISE,
                  Manifest.permission.BLUETOOTH_CONNECT,
                  Manifest.permission.ACCESS_WIFI_STATE,
                  Manifest.permission.CHANGE_WIFI_STATE,
                  Manifest.permission.NEARBY_WIFI_DEVICES,
          };

  private static final int REQUIRE_PERMISSIONS_FOR_APP_REQUEST_CODE = 0;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    requestPermissionsIfNeeded(
            this, REQUIRED_PERMISSIONS_FOR_APP, REQUIRE_PERMISSIONS_FOR_APP_REQUEST_CODE);

    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);

    ActionBar actionBar = getSupportActionBar();
    // showing the back button in action bar
    if (actionBar != null) {
      actionBar.setDisplayHomeAsUpEnabled(true);
    }
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

  public void debugAddEndpoint(View view) {
    MainViewModel.shared.addEndpoint(new EndpointViewModel("FOO", "BAR"));
  }

  public void debugChangeEndpoint(View view) {
    MainViewModel.shared.setLocalEndpointId("foo");
    MainViewModel.shared.setLocalEndpointName("Blah");
  }
}