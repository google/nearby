package com.google.hellocloud;

import static com.google.hellocloud.Utils.TAG;

import android.content.ContentResolver;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.fragment.app.DialogFragment;
import com.google.hellocloud.databinding.DialogFragmentImageViewBinding;

public class ImageViewDialogFragment extends DialogFragment {

  private Uri imageUri;

  public ImageViewDialogFragment(Uri imageUri) {
    this.imageUri = imageUri;
  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    var binding = DialogFragmentImageViewBinding.inflate(inflater, container, false);

    try {
      ContentResolver resolver = Main.shared.context.getContentResolver();
      Bitmap bitmap = MediaStore.Images.Media.getBitmap(resolver, imageUri);
      binding.photo.setImageBitmap(bitmap); // Setting generated QR code to imageView
    } catch (Exception e) {
      Log.e(TAG, "Failed to load image");
    }

    View view = binding.getRoot();
    view.setOnClickListener(
            v -> getDialog().dismiss());
    return view;
  }
}
