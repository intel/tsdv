/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

package com.intel.otc.tsdv;

import android.content.Context;
import android.util.AttributeSet;
import android.webkit.WebView;


/**
 * Our custom WebView used for displaying the graphs
 */
public class GraphView extends WebView {
    Context mContext;

    public GraphView(Context context) {
        super(context);
        mContext = context;
    }

    public GraphView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public GraphView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    public boolean overScrollBy(int deltaX, int deltaY, int scrollX, int scrollY,
                                int scrollRangeX, int scrollRangeY, int maxOverScrollX,
                                int maxOverScrollY, boolean isTouchEvent) {
        return false;
    }
}
