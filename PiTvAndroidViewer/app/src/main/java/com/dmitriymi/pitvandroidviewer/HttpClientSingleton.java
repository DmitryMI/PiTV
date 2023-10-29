package com.dmitriymi.pitvandroidviewer;

import android.content.Context;

import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.toolbox.Volley;

public class HttpClientSingleton
{
    private static HttpClientSingleton instance;
    private RequestQueue requestQueue;

    private HttpClientSingleton(Context context)
    {
        requestQueue = getRequestQueue(context);
    }

    public static synchronized HttpClientSingleton getInstance(Context context)
    {
        if (instance == null)
        {
            instance = new HttpClientSingleton(context);
        }
        return instance;
    }

    public RequestQueue getRequestQueue(Context ctx)
    {
        if (requestQueue == null)
        {
            // getApplicationContext() is key, it keeps you from leaking the
            // Activity or BroadcastReceiver if someone passes one in.
            requestQueue = Volley.newRequestQueue(ctx.getApplicationContext());
        }
        return requestQueue;
    }

    public <T> void addToRequestQueue(Context ctx, Request<T> req)
    {
        getRequestQueue(ctx).add(req);
    }

}
