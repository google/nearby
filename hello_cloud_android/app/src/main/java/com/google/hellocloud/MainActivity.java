package com.google.hellocloud;

import android.os.Bundle;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
  @Override
  protected void onCreate(Bundle savedInstanceState) {
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