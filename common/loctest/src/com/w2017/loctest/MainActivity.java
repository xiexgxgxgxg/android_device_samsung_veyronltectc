package com.w2017.loctest;

import android.app.Activity;
import android.content.Context;
import android.location.Location;
import android.location.Geocoder;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.Looper;
import android.util.Log;

public class MainActivity extends Activity {
    private static final String TAG = "LocTest";

    private final LocationListener mListener = new LocationListener() {
        @Override
        public void onLocationChanged(Location loc) {
            Log.i(TAG, "LOC " + loc.getProvider() + " " + loc.getLatitude() + ","
                    + loc.getLongitude() + " acc=" + loc.getAccuracy() + " t=" + loc.getTime());
        }
        @Override public void onStatusChanged(String p, int s, Bundle b) {}
        @Override public void onProviderEnabled(String p) { Log.i(TAG, "enabled " + p); }
        @Override public void onProviderDisabled(String p) { Log.i(TAG, "disabled " + p); }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LocationManager lm = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
        for (String p : new String[] { "fused", LocationManager.NETWORK_PROVIDER, LocationManager.GPS_PROVIDER }) {
            try {
                lm.requestLocationUpdates(p, 1000, 0, mListener, Looper.getMainLooper());
                Log.i(TAG, "requested " + p);
            } catch (Exception e) {
                Log.e(TAG, "request " + p + " failed", e);
            }
        }
        Log.i(TAG, "last known network=" + lm.getLastKnownLocation(LocationManager.NETWORK_PROVIDER));
        for (int n : new int[] { 1, 3, 10 }) {
            try {
                java.util.List<android.location.Address> addrs = new Geocoder(this).getFromLocation(43.8367489, 125.2367979, n);
                Log.i(TAG, "GEO n=" + n + " count=" + addrs.size());
                for (android.location.Address a : addrs) {
                    Log.i(TAG, "GEO n=" + n + " " + a.getCountryName() + "/" + a.getAdminArea() + "/" + a.getLocality()
                            + "/" + a.getSubLocality() + "/" + a.getThoroughfare() + " lat=" + a.getLatitude() + " lon=" + a.getLongitude());
                }
            } catch (Exception e) {
                Log.e(TAG, "geocode n=" + n + " failed", e);
            }
        }
    }
}
