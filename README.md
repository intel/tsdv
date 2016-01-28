# TSDV

## Overview
TSDV is a visualization and data filter library intended to be used on multiple platforms, including Android and iOS.
```
                .--------------------------------.
               |           TSDV           |
  .---------.  |  .----------------------------. |
  |         |  |  |    .------------------.    | |
  |   3rd   |  |  |    |   HTML/CSS/JS*   |    | |
  |  party  |  |  |    `------------------'    | |
  |   app   |  |  |   TSDV GraphView**  | |
  |         |<--->|     (Extended WebView)     | |
  |         |  |  `----------------------------' |
  |         |  |  .----------------------------. |
  |         |<--->| Pre-fetch filter Library** | |
  `-----^---'  |  `--------------^-------------' |
        |      '-----------------|---------------'
  .-----v------------------------v---------------.
  |                  SQL Lite                    |
  `----------------------------------------------'
```

TSDV is made up of three main parts:

1. TSDV GraphView
2. HTML/CSS/JS
3. Pre-fetch filter library

The TSDV GraphView is the main component instantiated into applications. It provides the rendering of and interaction with the data. Data is obtained via callbacks into the 3rd party application. Inside of the GraphView is an instance of the platform specific WebView control. That WebView control is extended with an additional JavaScript namespace `TSDV`. The optional d3js specific helper functions are provided by the `tsdvjs` module found in graph.js.

The HTML/CSS/JS loaded into the GraphView's WebView are standard HTML/JS/CSS resources. Any relative resource load requests by the HTML or JS are redirected into the application's execution data/ directory. The ability to load network resources is restricted by the platform's permission model. For example, if your application requests to sync with an external server, the application must request network access as part of its manifest.

The default UI provided by TSDV is implemented using [d3js](https://d3js.org). However this is not a requirement. Application writers are free to replace the HTML/JS/CSS with whatever sources they want.

The pre-fetch filter library is a native cross platform library developed using C/C++ and directly accesses the SQL Lite database populated by the application. It is used for pre-caching various levels-of-detail for summarizing the time series data stored in the SQL Lite database. This is used in order to minimize the amount of processing necessary at various zoom levels, thereby decreasing latency to the UI.

## Current Status
Currently we have a POC for Android (in the POC directory) and iOS (in the POC_iOS directory) which loads d3js and a sample data set all in HTML/JS.   The data set is stored in the SQLite database and pulled from a graph filter library implemented in native C++.  The Android POC also includes a custom build of SQLite (3.8) using the [amalgamation](https://www.sqlite.org/amalgamation.html) source file since it can't directly access the native SQLite library on Android.   This was done to validate the general architecture concepts for performance.

## GraphFilter API
Doxygen-generated API Documentation [PDF](./graphfilter/doc/latex/refman.pdf)

## Building
Feel free to change these if you find errors or a better way to do it

### Building for Android (with Android Studio)
#### System setup (Ubuntu and Mac)
##### Prerequisite
You'll need to download and setup Android Studio on your development platform and install the Android SDK and NDK in order to build the project.

To install Android Studio:
Visit the [Android Project page](https://developer.android.com/sdk/index.html)  and follow the instructions.

To install the Android SDK:
[Instructions](https://developer.android.com/sdk/installing/index.html?pkg=studio)

To install the Android NDK:
[Instructions](https://developer.android.com/ndk/downloads/index.html)

To Build the Library:
```bash
git clone https://github.com/01org/tsdv.git
cd tsdv/TSDVAndroidLib

Start up Android Studio and choose open existing project:

Navigate to tsdv/TSDVAndroidLib and open

Using project navigator on the left, open up local.properties file under Gradle Scripts, and set the ndk.dir to the path where you installed the Android NDK, for example:
ndk.dir=/Users/<username>/Library/Android/android-ndk-r10e

Click Try Again to sync local.properties file.
Click Build -> Make Project to build.

In the right hand pane clicke "Gradle". Search through the drop down options to find the 'assembleRelease' task, then execute it. This will build a .aar package in TSDVAndroidBridge/build/outputs/aar.

To Build the Sample App:

Open the SampleApp1 project in Android studio.

Import the library by clicking File->New Module->Import Jar/AAR Package. Choose the .aar generated above.

Attach a device through USB (or use emulator)
Click Run -> Run App and send it to the device of your choice
```
### Building for Android (using command line)
#### System setup (Ubuntu)
##### Prerequisite
The TSDV proof-of-concept is written to build with the Android SDK 4.4.2 (KitKat), which then allows the library to be used on Kit Kat and Lollipop. As such, you must first install the Android SDK. For information on doing so, visit the [AOSP project page](https://developer.android.com/sdk/installing/index.html?pkg=tools) and install the SDK).

Ensure you set the ANDROID_HOME environment variable to the location you install the Android SDK. For example:

```bash
export ANDROID_HOME=~/android/sdk
```

Once the SDK is installed, you also need to have a modern version of gradle installed. You can download the latest version from the [Gradle website](https://gradle.org/downloads/).

```bash
wget https://services.gradle.org/distributions/gradle-2.3-bin.zip
unzip gradle-2.3-bin.zip
export PATH=$PATH:${PWD}/gradle-2.3/bin
```
To determine if the path is set correctly, run:
```bash
gradle -v
```
If the above command exits with an error message about JAVA_HOME, see this [post on stackoverflow](http://stackoverflow.com/questions/22307516/gradle-finds-wrong-java-home-even-though-its-correctly-set).

*OPTIONAL:* If you are using a proxy, you need to configure your gradle properties:
```bash
cat > ~/.gradle/gradle.properties << EOF
    systemProp.http.proxyHost=$(echo ${http_proxy%:*} | sed -e "s,^http://,,g")
    systemProp.http.proxyPort=${http_proxy##*:}
    systemProp.http.nonProxyHosts=${no_proxy//,/|}

    systemProp.https.proxyHost=$(echo ${https_proxy%:*} | sed -e "s,^https://,,g")
    systemProp.https.proxyPort=${https_proxy##*:}
    systemProp.https.nonProxyHosts=${no_proxy//,/|}
EOF
```
### Build TSDV Android Library (using command line)
```bash
git clone https://github.com/01org/tsdv.git
cd tsdv/TSDVAndroidLib
gradle init
gradle assembleRelease
```

Please note: the current “preferred way” of starting a Gradle build is now to use the Gradle Wrapper – essentially a way of bundling gradle build instructions with your project that doesn’t require the developer to already have Gradle installed (let alone a specific version you used).  When we created our library project, Android Studio automatically included the wrapper in our project.  So using this approach, the build instructions would simply be:
./gradlew assembleRelease

You will now have a .aar file in TSDVAndroidBridge/build/outputs/aar. Copy (overwrite if exists) this file into tsdv/SampleApp1/TSDVAndroidLib.

Now navigate to SampleApp1, and type:
gradle build

You now will have debug versions of the .apk files in app/build/outputs/apk.

### Building for iOS (with Xcode)
##### Prerequisite
You need to download latest Xcode with support for iOS 9 from App store on your mac, and a iOS developer account is required to build and run on an actual device.  If you don't have an iOS developer account, you can only run it on the simulator.
```bash
git clone https://github.com/01org/tsdv.git
cd tsdv/SampleApp1_iOS

Start up Xcode
Open tsdv/SampleApp1_iOS/SampleApp1.xcodeproj

To build and run on the actual device:

 1. Create your code signing identity and team provisioning profile
 2. Connect a device (iPhone or iPad)
 2. Using the project navigator window on the left, click on the Data Viz POC project, go to General tab and make sure Deployment Target field matches or is lower than the version of iOS installed on your device
 4. Select Product -> Destination -> (actual device)
 5. Select Proejct -> Build to build
 5. Select Product -> Run to install the app and run on the device
```

To build and run on the simulator:

 1. Open the tsdv/TSDViOSLib/TSDViOSLib.xcodeproj project in xcode
 2. Select Product -> Destination -> (choice of devices)
 3. Select Product -> Build to build
 4. Use Finder to locate the generated TSDViOSLib.framework directory
 5. Copy and move the generated framework to tsdv/SampleApp1_iOS/TSDViOSLib.framework (replace existing one)
 6. Open the tsdv/SampleApp1_iOS/SampleApp1.xcodeproj project in xcode
 7. Select Product -> Destination -> (choice of devices)
 8. Select Product -> Build to build
 9. Select Product -> Run to run on the simulator

Please note: Since you can only select to build one architecture one at a time for the framework library, if you want to switch between running on an actual device or the simular, you will have to first build the framework library with the selected platform and then copy over the framework, and rebuild the application with matching platform. To locate the framework library after building the TSDViOSLib project, select the Products folder in the project navigator and right-click on the framework and click Show in Finder.  The default directory should be something like ~/Library/Developer/Xcode/DerivedData/TSDViOSLib-gxgyqgwsnzhsqwfgntvwxzynpmuw/Build/Products/Debug-iphoneos/ or ~/Library/Developer/Xcode/DerivedData/TSDViOSLib-gxgyqgwsnzhsqwfgntvwxzynpmuw/Build/Products/debug-iphonesimulator/ depending on if you choose to build with an actual device or the simulator.  Then copy the framework to the directory SampleApp1_SampleApp1_iOS/TSDViOSLib.framework, and then rebuilt your app.

Currently the sample app is set to search the SampleApp1_iOS/TSDViOSLib.framework sub directory.  If you are putting the framework library in another location, make sure you update the "Framework Search Path" field in the project settings to reflect the actual framwork location.

## Unit Tests
Units Tests are done using Google Test framework and integrated into the Android POC running on the Android Platform.
### Building and Running Unit Tests (with Android Studio Only)
##### Prerequisite
1. Follow the instructions to build the Android POC using Android Studio, the Unit Tests will be built as well.
2. Attach a Android Device

Then open up an terminal and navigate to the project directory, and run the following script from the terminal:
```bash
export PATH=/Users/<user name>/Library/Android/sdk/platform-tools:$PATH (Optional, for setting up adb if it not in your $PATH, ie. when running on Mac OSX)
./run_unit_tests_android.sh
```
