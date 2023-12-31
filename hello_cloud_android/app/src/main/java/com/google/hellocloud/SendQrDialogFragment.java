package com.google.hellocloud;

import static com.google.hellocloud.Utils.TAG;

import android.graphics.Bitmap;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.fragment.app.DialogFragment;
import com.google.hellocloud.databinding.DialogFragmentSendQrBinding;
import com.google.zxing.BarcodeFormat;
import com.google.zxing.MultiFormatWriter;
import com.google.zxing.WriterException;
import com.google.zxing.common.BitMatrix;
import com.journeyapps.barcodescanner.BarcodeEncoder;

public class SendQrDialogFragment extends DialogFragment {
  private String qrString;

  public SendQrDialogFragment(String qrString) {
    this.qrString = qrString;
  }

//  @Override
//  public void onCreate(Bundle savedInstanceState) {
//    super.onCreate(savedInstanceState);
//    if (getArguments() != null) {
//      qrString = getArguments().getString("qr_string");
//    }
//  }

  @Override
  public View onCreateView(
      LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    var binding = DialogFragmentSendQrBinding.inflate(inflater, container, false);

    MultiFormatWriter writer = new MultiFormatWriter();
    try {
      BitMatrix matrix = writer.encode(qrString, BarcodeFormat.QR_CODE, 800, 800);
      BarcodeEncoder encoder = new BarcodeEncoder();
      Bitmap bitmap = encoder.createBitmap(matrix); // creating bitmap of code
      binding.qrCode.setImageBitmap(bitmap); // Setting generated QR code to imageView
    } catch (WriterException e) {
      Log.e(TAG, "Failed to encode QR code");
    }

    return binding.getRoot();
  }
}
