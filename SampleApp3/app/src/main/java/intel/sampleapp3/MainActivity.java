package intel.sampleapp3;

import android.support.v7.app.ActionBarActivity;
import android.os.Bundle;
import android.webkit.WebSettings;

import com.intel.otc.tsdv.GraphView;
import com.intel.otc.tsdv.JavascriptBridge;
import com.intel.otc.tsdv.TSDVWebViewClient;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends ActionBarActivity {
    static final String DATABASE_NAME = "2MonthData.db";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        GraphView graphview = (GraphView) findViewById(R.id.graphview);
        WebSettings graphviewSettings = graphview.getSettings();
        graphviewSettings.setJavaScriptEnabled(true);
        graphviewSettings.setAllowFileAccess(true);
        graphviewSettings.setAllowFileAccessFromFileURLs(true);
        graphviewSettings.setAllowUniversalAccessFromFileURLs(true);
        GraphView.setWebContentsDebuggingEnabled(true);

        String cacheSetup = "{\"useCache\": true," +
                "\"cacheRawData\": true," +
                "\"downsamplingFilter\": \"TIME_WEIGHTED_POINTS\"," +
                "\"fetchAhead\": 1," +
                "\"fetchBehind\": 1," +
                "\"downsamplingLevels\": [" +
                "   { \"duration\": 86400," +
                "     \"numOfPoints\": 100" +
                "   }" +
                " ]" +
                "}";

        String dataSchema = "{\"table\": \"data\"," +
                "\"date_key_column\": \"date\"," +
                "\"columns\": {" +
                "   \"date\": \"TEXT\"," +
                "   \"calories\": \"REAL\"," +
                "   \"heart_rate\": \"INT\"," +
                "   \"body_temp\": \"REAL\"," +
                "   \"steps\": \"INT\"," +
                "   \"blood_sugar\": \"REAL\"" +
                " }" +
                "}";

        File cacheDir = this.getCacheDir();
        String databaseFilePath = cacheDir.getAbsolutePath() + File.separator + DATABASE_NAME;
        if (cacheDir.canRead()){
            File db = new File(databaseFilePath);
            if(!db.exists()){
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
}
