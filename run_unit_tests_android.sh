#!/bin/sh

find ./TSDVAndroidLib/TSDVAndroidBridge/src/main/libs/armeabi-v7a -type f -print -exec adb push {} /data/local/tmp \;

adb shell LD_LIBRARY_PATH=/data/local/tmp /data/local/tmp/graphfilter_unittest
