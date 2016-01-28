/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

package com.intel.otc.tsdv;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.preference.PreferenceManager;
import android.util.Log;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileFilter;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.regex.Pattern;

/**
 * A listener interface that can be implemented by an app if it wants to receive signals
 * from the GraphView.
 */
interface TSDVSignalListner {
    void onSignalReceived(String signalName, String values);
}

/**
 * This class contains the APIs for communication between the GraphView and the
 * native code layer.
 */
public class JavascriptBridge {
    Context mContext;
    File logFile;
    GraphView webView;
    GraphFilterJNILib graphFilter;
    boolean enableLogging;
    TSDVSignalListner signalListener = null;

    public JavascriptBridge(Context context, GraphView wView, String cacheSetup, String databasePath, String dataSchema, boolean cleanDatabase) {
        mContext = context;
        webView = wView;

        graphFilter = new GraphFilterJNILib();
        File cacheDir = context.getCacheDir();

        if (loggingOptionEnabled("LOGGING_ENABLED"))
            initLogFile();

        // Make sure we have access to the application cache directory
        if (cacheDir.canWrite()) {
            Log.d("JavascriptBridge", "Cache File Dir: " + cacheDir + " is writable");

            if (!graphFilter.init(cacheSetup, dataSchema, databasePath, cleanDatabase)) {
                Log.e("JavascriptBridge", "Cannot initialize graph filter");
                return;
            }
        } else {
            Log.e("JavascriptBridge", "No write access to directory: " + cacheDir.getAbsolutePath());
        }
    }

    public void setSignalListener(TSDVSignalListner listener) {
        signalListener = listener;
    }

    private void initLogFile() {
        File fileDir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).toString());
        fileDir.mkdirs();

        if (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState())) {
            Calendar c = Calendar.getInstance();
            SimpleDateFormat df = new SimpleDateFormat("dd-MMM-yyyy-HH:mm:ss");
            String formattedDate = df.format(c.getTime());

            logFile = new File(fileDir, "Android-" + formattedDate + ".txt");

            try {
                FileOutputStream outStream = new FileOutputStream(logFile);
                PrintWriter pw = new PrintWriter(outStream);
                pw.println("timestamp,duration,datasize,method");
                pw.flush();
                pw.close();
                outStream.close();
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            }

            //Scan so file appears
            MediaScannerConnection.scanFile(mContext,
                    new String[]{logFile.toString()}, null,
                    new MediaScannerConnection.OnScanCompletedListener() {
                        public void onScanCompleted(String path, Uri uri) {
                            Log.i("TSDV", "Scanned " + path + ":");
                            Log.i("TSDV", "-> uri=" + uri);
                        }
                    });
        }
    }

    private void callJSCallback(final String jsCallback) {
        Handler mainHandler = new Handler(Looper.getMainLooper());
        ((Activity) mContext).runOnUiThread(new Runnable() {
            @Override
            public void run() {
                webView.evaluateJavascript(jsCallback, new ValueCallback<String>() {
                    @Override
                    public void onReceiveValue(String value) {
                    }
                });
            }
        });
    }

    /**
     * Request data from native database
     */
    public void nativeGetData(String paramsStr, String jsCallback, String argsStr, String jsErrorCB) {
        /** calling native C++ library through JNI */
        JSONObject params;
        JSONObject returnObject;
        String startDate, endDate;
        Integer maxLength;
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm'Z'");

        try {
            params = new JSONObject(paramsStr);
            startDate = params.getString("startDate");
            endDate = params.getString("endDate");
            maxLength = params.getInt("numOfPoints");

            if (startDate.isEmpty() || endDate.isEmpty()) {
                callJSCallback(jsCallback + "('" + "No startDate or endDate provided" + "')");
                return;
            }

            String result = graphFilter.getData(paramsStr);

            if (result != null && !result.isEmpty()) {
                returnObject = new JSONObject(result);
                callJSCallback(jsCallback + "('" + returnObject.toString() + "','" + argsStr + "')");
            } else if (jsErrorCB != null)
                callJSCallback(jsErrorCB + "('" + "Failed to find any data in that range" + "')");
        } catch (JSONException e) {
            e.printStackTrace();
        }
        return;
    }

    //Used for url data requests
    public String syncGetData(JSONObject params) {
        /** calling native C++ library through JNI */
        JSONObject returnObject = new JSONObject();
        String startDate, endDate;
        Integer maxLength;

        try {
            startDate = params.getString("startDate");
            endDate = params.getString("endDate");
            maxLength = params.getInt("numOfPoints");

            if (startDate.isEmpty() || endDate.isEmpty()) {
                returnObject.toString();
            }

            String result = graphFilter.getData(params.toString());

            if (result != null && !result.isEmpty()) {
                returnObject = new JSONObject(result);
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }

        return returnObject.toString();
    }

    /**
     * Show a toast message from the web page
     */
    @JavascriptInterface
    public void showToast(String toast) {
        Toast.makeText(mContext, toast, Toast.LENGTH_SHORT).show();
    }

    /**
     * Asynchronously request a new set of data from the DB
     */
    @JavascriptInterface
    public void loadData(final String optionsStr, final String jsCallback, final String argsStr, final String jsErrorCB) {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                nativeGetData(optionsStr, jsCallback, argsStr, jsErrorCB);
            }
        });
    }

    /**
     * Check if logging is enabled
     */
    @JavascriptInterface
    public boolean loggingOptionEnabled(String option) {
        SharedPreferences preference = PreferenceManager.getDefaultSharedPreferences(this.mContext);
        return preference.getBoolean(option, false);
    }

    /**
     * Enable / disable logging
     */
    @JavascriptInterface
    public void setLoggingOption(String option, boolean enable) {
        if (enable && logFile == null && option.matches("LOGGING_ENABLED"))
            initLogFile();

        SharedPreferences preference = PreferenceManager.getDefaultSharedPreferences(this.mContext);
        SharedPreferences.Editor editor = preference.edit();
        editor.putBoolean(option, enable);
        editor.commit();
    }

    /**
     * Delete old log files
     */
    @JavascriptInterface
    public void deleteOldLogs() {
        File logFolder = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).toString());

        if (logFolder.exists() && logFolder.isDirectory()) {
            final Pattern p = Pattern.compile("Android.*");

            File[] logFiles = logFolder.listFiles(new FileFilter() {
                @Override
                public boolean accept(File file) {
                    Boolean result = false;
                    if (p.matcher(file.getName()).matches() && (logFile == null || !file.getName().matches(logFile.getName())))   //If we find a log file and it's not the one we are currently logging to
                    {
                        try {
                            result = file.delete();
                            Log.i("TSDV", "removing log file - " + file.getName() + " : " + result);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                    return result;
                }
            });
            for (File deletedFile : logFiles) {
                MediaScannerConnection.scanFile(mContext,
                        new String[]{deletedFile.toString()}, null,
                        new MediaScannerConnection.OnScanCompletedListener() {
                            public void onScanCompleted(String path, Uri uri) {
                                Log.i("TSDV", "Re-scan " + path + ":");
                            }
                        });
            }
        }
    }

    /**
     * Log data to file
     */
    @JavascriptInterface
    public void logToFile(String time, int renderTime, int dataSize, String callFunc) throws IOException {
        String logStr = time + ',' + renderTime + ',' + dataSize + ',' + callFunc;
        if (logStr != null && logFile != null) {
            try {
                FileOutputStream outStream = new FileOutputStream(logFile, true);
                PrintWriter pw = new PrintWriter(outStream);
                pw.println(logStr);
                pw.flush();
                pw.close();
                outStream.close();

                Log.i("TSDV", "Logged " + logStr);
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * Emit a signal from the Javascript layer
     * signalName - The name of the signal
     * values - JSON string containing optional values that are passed with that signal
     */
    @JavascriptInterface
    public void emitSignal(String signalName, String values) {
        if (signalListener != null) {
            signalListener.onSignalReceived(signalName, values);
        }
        try {
            if (loggingOptionEnabled("LOGGING_ENABLED"))
                logToFile(signalName, 0, 0, values);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
