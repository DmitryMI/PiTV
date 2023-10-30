package com.dmitriymi.pitvandroidviewer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.util.Base64;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.TextView;
import android.widget.Toast;

import com.android.volley.AuthFailureError;
import com.android.volley.Request;
import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.JsonObjectRequest;

import org.freedesktop.gstreamer.GStreamer;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.Map;

public class CameraViewActivity extends AppCompatActivity implements SurfaceHolder.Callback
{
    private final int LEASE_TIME_MSEC = 10000;
    private final int LEASE_TIME_PERIOD_MSEC = 5000;

    // Loading C++ libs and initializing Java Native Interface
    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("rtp_pipeline");
        nativeClassInit();
    }

    // List of methods implemented in C++ but callable from Java
    private native void nativeInit();     // Initialize native code, build pipeline, etc
    private native void nativeFinalize(); // Destroy pipeline and shutdown native code
    private native void nativePlay();     // Set pipeline to PLAYING
    private native void nativePause();    // Set pipeline to PAUSED
    private native void nativeSetUdpPort(int port);
    private static native boolean nativeClassInit(); // Initialize native class: cache Method IDs for callbacks
    private native void nativeSurfaceInit(Object surface);
    private native void nativeSurfaceFinalize();
    private long native_custom_data;      // Native code will use this to keep private data
    private ServerConfig serverConfig;
    private String guid;
    Handler autoLeaseHandler = new Handler();

    private final Runnable autoLeaseRunnable = new Runnable() {
        @Override
        public void run() {
            // Execute tasks on main thread
            Log.d("autoLeaseRunnable", "Called on main thread");
            // Repeat this task again another 2 seconds
            // autoLeaseHandler.postDelayed(this, 2000);
            requestCameraLease();
        }
    };

    private static class JsonObjectRequestBasicAuth extends JsonObjectRequest
    {
        String username;
        String password;
        public JsonObjectRequestBasicAuth(
                int method,
                String url,
                @Nullable JSONObject jsonRequest,
                Response.Listener<JSONObject> listener,
                @Nullable Response.ErrorListener errorListener)
        {
            super(
                    method,
                    url,
                    jsonRequest,
                    listener,
                    errorListener);
        }

        @Override
        public Map<String, String> getHeaders() throws AuthFailureError {
            HashMap<String, String> params = new HashMap<String, String>();
            String creds = String.format("%s:%s",username,password);
            String auth = "Basic " + Base64.encodeToString(creds.getBytes(), Base64.DEFAULT);
            params.put("Authorization", auth);
            return params;
        }
    }

    private boolean requestCameraLease()
    {
        if(serverConfig == null || serverConfig.serverUrl.isEmpty())
        {
            return false;
        }

        String leaseGuid;
        if(guid != null)
        {
            leaseGuid = guid;
        }
        else
        {
            leaseGuid = "";
        }

        JSONObject requestData = new JSONObject();
        try
        {
            requestData.put("lease_guid", leaseGuid);
            requestData.put("udp_address", serverConfig.getLocalUdpAddress());
            requestData.put("udp_port", serverConfig.getLocalUdpPort());
            requestData.put("lease_time", LEASE_TIME_MSEC);
        }
        catch (JSONException e)
        {
            throw new RuntimeException(e);
        }

        JsonObjectRequestBasicAuth jsonObjectRequest = new JsonObjectRequestBasicAuth(Request.Method.POST, serverConfig.serverUrl, requestData,
                response ->
                {
                    onRequestCameraLeaseSuccessful(response);
                    Log.i("requestCameraLease", "Lease request was successful");
                },
                error ->
                {
                    // TODO: Handle error
                    String message = error.getMessage();
                    if(message != null) {
                        Log.e("requestCameraLease", "Lease request failed: " + error.getMessage());
                    }
                    else {
                        Log.e("requestCameraLease", "Lease request failed: " + error);
                    }
                    // Stop auto lease timer
                    autoLeaseHandler.removeCallbacks(autoLeaseRunnable);
                });

        jsonObjectRequest.username = serverConfig.username;
        jsonObjectRequest.password = serverConfig.password;
        Log.i("requestCameraLease", "Requesting camera lease from " + serverConfig.serverUrl);
        HttpClientSingleton.getInstance(this).addToRequestQueue(getApplicationContext(), jsonObjectRequest);
        return true;
    }

    private void onRequestCameraLeaseSuccessful(JSONObject response)
    {
        try
        {
            guid = response.getString("guid");

            if(!autoLeaseHandler.hasCallbacks(autoLeaseRunnable))
            {
                Log.i("onRequestCameraLeaseSuccessful", "Starting Auto Lease Runnable");
                autoLeaseHandler.postDelayed(autoLeaseRunnable, LEASE_TIME_PERIOD_MSEC);
            }
        }
        catch (JSONException e)
        {
            autoLeaseHandler.removeCallbacks(autoLeaseRunnable);
            throw new RuntimeException(e);
        }
    }

    // Called from C++ code. Use this to display Gstreamer error messages.
    private void setMessage(final String message) {

        final TextView tv = (TextView) this.findViewById(R.id.textview_gstreamer_message);
        runOnUiThread (new Runnable() {
            public void run() {
                tv.setText(message);
            }
        });

    }

    // Called from C++ code. C++ code calls this once it has created its pipeline.
    private void onGStreamerInitialized () {
        Log.i ("GStreamer", "Gst initialized.");

        // Set UDP port for the pipeline to listen to
        nativeSetUdpPort(serverConfig.getLocalUdpPort());

        // We want the pipeline to start as soon as it is ready
        nativePlay();
    }

    // Called from Surface View to inform us that the dimensions changed
    public void surfaceChanged(SurfaceHolder holder, int format, int width,
                               int height) {
        Log.d("GStreamer", "Surface changed to format " + format + " width "
                + width + " height " + height);

        // Inform Gstreamer about the changes
        nativeSurfaceInit (holder.getSurface());
    }

    // Called from Surface View to inform us that it is ready
    public void surfaceCreated(SurfaceHolder holder) {
        Log.d("GStreamer", "Surface created: " + holder.getSurface());
    }

    // Called from Surface View to inform us that it is destroyed
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
        Log.d("GStreamer", "Surface destroyed");
        nativeSurfaceFinalize ();
    }

    // Called when this Activity is destroyed (closed or offloaded from memory)
    // Gstreamer pipeline is freed in this method
    // Do not save Activity state here!
    // See https://developer.android.com/reference/android/app/Activity#onDestroy()
    @Override
    protected void onDestroy() {
        nativeFinalize();
        super.onDestroy();
    }

    // In this function we will save the Activity state
    protected void onSaveInstanceState (@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        Log.d ("GStreamer", "Saving state.");
        outState.putParcelable("serverConfig", serverConfig);
    }

    // Called when the Activity is first created. Most of initialization code should be here
    // See https://developer.android.com/reference/android/app/Activity#onCreate(android.os.Bundle)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Try to initialize Gstreamer
        try
        {
            GStreamer.init(this);
        }
        catch (Exception e)
        {
            Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        // Setup SurfaceView callbacks
        SurfaceView sv = (SurfaceView) this.findViewById(R.id.surfaceview_gstreamer);
        SurfaceHolder sh = sv.getHolder();
        sh.addCallback(this);

        // Restore Activity state if we had one
        // We do not call nativeSetUdpPort(port) here, because the Gstreamer pipeline is
        // not initialized yet.
        if (savedInstanceState != null)
        {
            serverConfig = savedInstanceState.getParcelable("serverConfig");
            Log.i ("GStreamer", "Activity created from saved state");
        } else {
            Log.i ("GStreamer", "Activity created. There is no saved state");
        }

        if(serverConfig == null)
        {
            serverConfig = new ServerConfig();
            serverConfig.serverUrl = "https://10.0.2.2:8080";
            serverConfig.username = "Dmitriy";
            serverConfig.password = "6032403";

            String ipGuess = NetworkUtils.getIPAddress(true);
            serverConfig.localUdpEndpoint = ipGuess + ":5000";
            Log.i("onCreate", "Selected local UDP endpoint: " + serverConfig.localUdpEndpoint);
        }

        if(requestCameraLease())
        {
            Log.i("onCreate", "Sent camera lease request from onCreate");
            // Initialize the pipeline
            nativeInit();
        }
        else {
            Log.i("onCreate", "Failed to request camera lease during onCreate execution");
        }
    }
}