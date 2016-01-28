/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

package intel.poc;

import android.app.Activity;
import android.content.Context;
import android.support.v7.app.ActionBar;
import android.support.v7.app.ActionBarActivity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.webkit.WebView;
import android.webkit.WebSettings;
import android.webkit.WebChromeClient;
import android.util.Log;

import com.intel.otc.tsdv.GraphView;
import com.intel.otc.tsdv.JavascriptBridge;
import com.intel.otc.tsdv.TSDVWebViewClient;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * This is the main class of the Sample app.  It declares and
 * initializes the GraphView.
 */
public class Graphs extends Activity {
    static final String DATABASE_NAME = "stock.db";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_graphs);
        android.app.ActionBar actionBar = getActionBar();
        actionBar.hide();
        GraphView graphview = (GraphView) findViewById(R.id.graphview);
        WebSettings graphviewSettings = graphview.getSettings();
        graphviewSettings.setJavaScriptEnabled(true);
        graphviewSettings.setAllowFileAccess(true);
        graphviewSettings.setAllowFileAccessFromFileURLs(true);
        graphviewSettings.setAllowUniversalAccessFromFileURLs(true);
        GraphView.setWebContentsDebuggingEnabled(true);


        String cacheSetup = "{\"useCache\": true," +
                "\"cacheRawData\": true," +
                "\"downsamplingFilter\": \"POINTS\"," +
                "\"downsamplingLevels\": [" +
                "   { \"duration\": 2629746000," +
                "     \"numOfPoints\": 31" +
                "   }," +
                "   { \"duration\": 7889000000," +
                "     \"numOfPoints\": 40" +
                "   }," +
                "   { \"duration\": 15778476000," +
                "     \"numOfPoints\": 40" +
                "   }," +
                "   { \"duration\": 31556952000," +
                "     \"numOfPoints\": 70" +
                "   }," +
                "   { \"duration\": 157784760000," +
                "     \"numOfPoints\": 100" +
                "   }," +
                "   { \"duration\": 315569520000," +
                "     \"numOfPoints\": 200" +
                "   }" +
                " ]" +
                "}";

        String dataSchema = "{\"table\": \"data\"," +
                "\"date_key_column\": \"Date\"," +
                "\"columns\": {" +
                "   \"Date\": \"TEXT\"," +
                "   \"Open\": \"REAL\"," +
                "   \"High\": \"REAL\"," +
                "   \"Low\": \"REAL\"," +
                "   \"Close\": \"REAL\"," +
                "   \"Volume\": \"INT\"" +

                " }" +
                "}";


        File cacheDir = this.getCacheDir();
        String databaseFilePath = cacheDir.getAbsolutePath() + File.separator + DATABASE_NAME;
        if (cacheDir.canRead()) {
            File db = new File(databaseFilePath);
            if (!db.exists()) {
                if (cacheDir.canWrite()) {
                    InputStream inputStream = null;
                    try {
                        inputStream = this.getAssets().open(DATABASE_NAME);
                        OutputStream outputStream = new FileOutputStream(databaseFilePath);

                        byte[] buffer = new byte[8192]; // 8kB
                        int length = inputStream.read(buffer);

                        while (length > 0) {
                            outputStream.write(buffer, 0, length);
                            length = inputStream.read(buffer);
                        }
                        outputStream.flush();
                        outputStream.close();
                        inputStream.close();
                    } catch (IOException e) {
                        throw new RuntimeException("Error copying database file to cache");
                    }
                } else {
                    throw new RuntimeException("Can't write database to cache path");
                }
            }
        } else {
            throw new RuntimeException("Can't read from cache path");
        }

        JavascriptBridge myJB = new JavascriptBridge(this, graphview, cacheSetup, databaseFilePath, dataSchema, false);
        graphview.addJavascriptInterface(myJB, "TSDV");
        graphview.setWebViewClient(new TSDVWebViewClient(myJB));
        graphview.loadUrl("file:///android_asset/index.html");
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_graphs, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
