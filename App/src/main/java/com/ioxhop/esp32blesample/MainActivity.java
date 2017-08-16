package com.ioxhop.esp32blesample;

import android.Manifest;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.ToggleButton;

import org.w3c.dom.Text;

import java.util.List;
import java.util.Timer;
import java.util.TimerTask;
import java.util.UUID;

public class MainActivity extends AppCompatActivity {

    public ToggleButton toggleButton;
    public TextView textView;

    private static final int PERMISSION_REQUEST_COARSE_LOCATION = 1;

    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothGatt mBluetoothGatt;

    BluetoothGattCharacteristic mGattCharGetTemperature;
    BluetoothGattCharacteristic mGattCharWriteLED;

    private static final int REQUEST_ENABLE_BT = 200;
    Timer timer;

    ProgressDialog progress;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        toggleButton = (ToggleButton) findViewById(R.id.toggleButton);
        textView = (TextView) findViewById(R.id.textView);

        // toggleButton.setEnabled(false);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // Android M Permission check 
            if (this.checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                final AlertDialog.Builder builder = new AlertDialog.Builder(this);
                builder.setTitle("This app needs location access");
                builder.setMessage("Please grant location access so this app can detect beacons.");
                builder.setPositiveButton(android.R.string.ok, null);
                builder.setOnDismissListener(new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(DialogInterface dialog) {
                        requestPermissions(new String[]{Manifest.permission.ACCESS_COARSE_LOCATION}, PERMISSION_REQUEST_COARSE_LOCATION);
                    }
                });
                builder.show();
                return;
            }
        }
        beginBLE();
    }

    @Override
    protected void onResume() {
        super.onResume();

    }

    public void beginBLE() {
        // BLE
        final BluetoothManager bluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        mBluetoothAdapter = bluetoothManager.getAdapter();
        if (mBluetoothAdapter == null || !mBluetoothAdapter.isEnabled()) {
            Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
        } else {
            scanLeDevice(true);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();

        if (mBluetoothAdapter != null && mBluetoothAdapter.isEnabled()) {
            scanLeDevice(false);
        }
        if (mBluetoothGatt != null) {
            mBluetoothGatt.close();
        }
        if (timer != null) timer.cancel();
    }


    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (mBluetoothGatt == null) {
            return;
        }
        mBluetoothGatt.close();
        mBluetoothGatt = null;
    }

    public void ontoggleButtonClick(View view) {
        Log.i("ontoggleButtonClick", "Now toggle is " + toggleButton.isChecked());
        if (mGattCharWriteLED != null) {
            Log.i("ontoggleButtonClick", "Send !!!");
            mGattCharWriteLED.setValue(toggleButton.isChecked() ? 1 : 0,
                    BluetoothGattCharacteristic.FORMAT_UINT8, 0);
            mBluetoothGatt.writeCharacteristic(mGattCharWriteLED);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        // User chose not to enable Bluetooth.
        Log.i("onActivityResult", "requestCode = " + requestCode);
        Log.i("onActivityResult", "resultCode = " + resultCode);
        if (requestCode == REQUEST_ENABLE_BT) {
            if (mBluetoothAdapter == null || !mBluetoothAdapter.isEnabled()) {
                finish();
                return;
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String permissions[],
                                           int[] grantResults) {
        switch (requestCode) {
            case PERMISSION_REQUEST_COARSE_LOCATION: {
                if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    Log.d("onRequestPermis...", "coarse location permission granted");
                    beginBLE();
                } else {
                    final AlertDialog.Builder builder = new AlertDialog.Builder(this);
                    builder.setTitle("Functionality limited");
                    builder.setMessage("Since location access has not been granted, this app will not be able to discover beacons when in the background.");
                    builder.setPositiveButton(android.R.string.ok, null);
                    builder.setOnDismissListener(new DialogInterface.OnDismissListener() {
                        @Override
                        public void onDismiss(DialogInterface dialog) {
                        }
                    });
                    builder.show();
                }
                return;
            }
        }
    }

    private void scanLeDevice(final boolean enable) {
        final BluetoothLeScanner bluetoothLeScanner =
                mBluetoothAdapter.getBluetoothLeScanner();

        if (enable) {
            progress = ProgressDialog.show(this, "Scaning", "wait a min.");

            bluetoothLeScanner.startScan(mLeScanCallback);
            Log.i("scanLeDevice", "Start scan");

        } else {
            // if (progress.isIndeterminate()) progress.dismiss();

            bluetoothLeScanner.stopScan(mLeScanCallback);
            Log.i("scanLeDevice", "Stop scan");
        }
    }

    private ScanCallback mLeScanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            super.onScanResult(callbackType, result);

            Log.i("onScanResult", result.getDevice().getAddress());
            Log.i("onScanResult", result.getDevice().getName());

            scanLeDevice(false);

            mBluetoothGatt = result.getDevice().connectGatt(getApplicationContext(), false, mBluetoothGattCallback);
        }
    };

    public final BluetoothGattCallback mBluetoothGattCallback = new BluetoothGattCallback() {
        @Override
        public void onCharacteristicRead (BluetoothGatt gatt,
                                          BluetoothGattCharacteristic characteristic,
                                          int status) {
            Log.i("onCharacteristicRead", characteristic.getStringValue(0));
            // mBluetoothGatt.disconnect();

            // textView.setText("Temperature: " + characteristic.getStringValue(0) + " *C");
            setText(textView, "Temperature: " + characteristic.getStringValue(0) + " *C");
        }

        @Override
        public void onCharacteristicWrite (BluetoothGatt gatt,
                                           BluetoothGattCharacteristic characteristic,
                                           int status) {
            Log.i("onCharacteristicWrite", characteristic.getStringValue(0));
            // mBluetoothGatt.disconnect();
        }

        @Override
        public void onConnectionStateChange (BluetoothGatt gatt,
                                             int status,
                                             int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                mBluetoothGatt.discoverServices();
            }
        }

        @Override
        public void onServicesDiscovered (BluetoothGatt gatt,
                                          int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                BluetoothGattService mGattService =
                        mBluetoothGatt.getService(UUID.fromString("000000ff-0000-1000-8000-00805f9b34fb"));
                if (mGattService != null) {
                    Log.i("onServicesDiscovered",
                            "Service characteristic UUID found: " + mGattService.getUuid().toString());

                    mGattCharGetTemperature =
                            mGattService.getCharacteristic(UUID.fromString("0000ff01-0000-1000-8000-00805f9b34fb"));

                    if (mGattCharGetTemperature != null) {
                        Log.i("onServicesDiscovered",
                                "characteristic UUID found: " + mGattCharGetTemperature.getUuid().toString());

                        // Set timer, read temperature every 2S
                        timer = new Timer();
                        timer.scheduleAtFixedRate(new TimerTask() {
                            @Override
                            public void run() {
                                mBluetoothGatt.readCharacteristic(mGattCharGetTemperature);
                            }
                        }, 5, 2000);

                    } else {
                        Log.i("onServicesDiscovered",
                                "characteristic not found for UUID: " + mGattCharGetTemperature.getUuid().toString());

                    }

                    mGattCharWriteLED =
                            mGattService.getCharacteristic(UUID.fromString("0000ff02-0000-1000-8000-00805f9b34fb"));

                    if (mGattCharWriteLED != null) {
                        Log.i("onServicesDiscovered",
                                "characteristic UUID found: " + mGattCharWriteLED.getUuid().toString());

                        toggleButton.setEnabled(true);
                    } else {
                        Log.i("onServicesDiscovered",
                                "characteristic not found for UUID: " + mGattCharWriteLED.getUuid().toString());

                    }
                } else {
                    Log.i("onServicesDiscovered",
                            "Service characteristic not found for UUID: " + mGattService.getUuid().toString());

                }

                if (progress.isIndeterminate()) progress.dismiss();
            }
        }
    };

    private void setText(final TextView text,final String value){
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                text.setText(value);
            }
        });
    }
}
