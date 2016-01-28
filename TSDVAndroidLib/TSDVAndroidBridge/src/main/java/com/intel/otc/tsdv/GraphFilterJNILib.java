/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

package com.intel.otc.tsdv;

// JNI Wrapper for C++ shared library

public class GraphFilterJNILib {

    // native methods, implementation is is in the JNI c++ lib
    private native boolean initNative(String cacheSetup, String dataSchema, String databasePath, boolean clean);

    private native boolean addDataNative(String dataValues);

    private native String getDataNative(String params);

    static {
        System.loadLibrary("graphfilter");
    }

    // wrapper methods that can be called by Java
    public boolean init(String cacheSetup, String dataSchema, String path, boolean clean) {
        return initNative(cacheSetup, dataSchema, path, clean);
    }

    public Boolean addData(String dataValues) {
        return addDataNative(dataValues);
    }

    public String getData(String params) {
        return getDataNative(params);
    }
}
