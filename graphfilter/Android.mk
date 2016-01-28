# Copyright 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)

## SQLite
include $(CLEAR_VARS)

LOCAL_MODULE            := sqlite3

LOCAL_SRC_FILES         := sqlite3/sqlite3.c

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/sqlite3

LOCAL_CFLAGS            := -DSQLITE_THREADSAFE=1

include $(BUILD_STATIC_LIBRARY)


## JsonCpp
include $(CLEAR_VARS)

LOCAL_MODULE            := jsoncpp

LOCAL_SRC_FILES         := jsoncpp/jsoncpp.cpp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/jsoncpp/json

LOCAL_CPPFLAGS          := -Werror -fexceptions

include $(BUILD_STATIC_LIBRARY)


## GraphFilter library
include $(CLEAR_VARS)

LOCAL_MODULE            := graphfilter

LOCAL_SRC_FILES         := src/graphfilter.cpp            \
                           src/databasegraphfilter.cpp    \
                           src/graphfilterjnilib.cpp      \
                           src/sqlitedatacache.cpp        \
                           src/datafilter.cpp             \
                           src/sqlitedatabaseaccess.cpp

LOCAL_C_INCLUDES        := $(LOCAL_PATH)/include

LOCAL_STATIC_LIBRARIES  := libsqlite3 libjsoncpp

LOCAL_CPPFLAGS          := -Werror -fexceptions

LOCAL_LDLIBS            := -llog 

include $(BUILD_SHARED_LIBRARY)


## Unit Tests
include $(CLEAR_VARS)

LOCAL_MODULE            := graphfilter_unittest

LOCAL_SRC_FILES         := tests/graphfilter_unittest.cpp

LOCAL_C_INCLUDES        := $(LOCAL_PATH)/include

LOCAL_SHARED_LIBRARIES  := libgraphfilter libjsoncpp

LOCAL_STATIC_LIBRARIES  := googletest_main

LOCAL_CFLAGS            := -fPIE

LOCAL_LDFLAGS           := -fPIE -pie

LOCAL_LDLIBS            := -llog

include $(BUILD_EXECUTABLE)

$(call import-module,third_party/googletest)
