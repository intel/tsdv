/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

package com.intel.otc.tsdv;

import android.util.Log;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayInputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;


//import android.webkit.WebResourceRequest;

public class TSDVWebViewClient extends WebViewClient {
    JavascriptBridge mJB;

    public TSDVWebViewClient(JavascriptBridge myJB) {
        mJB = myJB;
    }

    /**
     * Handles a URL get data request from the GraphView
     *
     * @param view - The Webview sending the request URL
     * @param url  - The URL containing the get data request parameters
     * @return
     */
    @Override
    public WebResourceResponse shouldInterceptRequest(final WebView view, String url) {
        if (url.contains("tsdv"))     //If the URL contains tsdv, parse it
        {
            String[] urlParsed = url.substring(23).split("&");
            JSONObject options = new JSONObject();

            for (int i = 0; i < urlParsed.length; i++) {
                Integer equalLoc = urlParsed[i].indexOf("=");
                try {
                    String paramName = urlParsed[i].substring(0, equalLoc);
                    String value = URLDecoder.decode(urlParsed[i].substring(equalLoc + 1), "UTF-8");

                    if (paramName.equals("startDate") || paramName.equals("endDate")) {
                        options.put(paramName, value);
                    } else if (paramName.equals("metrics")) {
                        String[] metrics = value.split(",");
                        options.put(paramName, new JSONArray(metrics));
                    } else if (paramName.equals("numOfPoints")) {
                        options.put(paramName, Integer.parseInt(value));
                    } else {
                        Log.e("GraphView", "Unsupported parameter");
                        /* TODO: handle error here */
                    }
                } catch (JSONException e) {
                    e.printStackTrace();
                } catch (UnsupportedEncodingException e) {
                    e.printStackTrace();
                }
            }

            String result = mJB.syncGetData(options);
            ByteArrayInputStream dasStream = new ByteArrayInputStream(result.getBytes());
            return new WebResourceResponse("application/json", "UTF-8", dasStream);
        } else {
            return super.shouldInterceptRequest(view, url);
        }
    }
}
