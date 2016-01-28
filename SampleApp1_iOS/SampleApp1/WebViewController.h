/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef Data_Viz_POC_WebViewController_h
#define Data_Viz_POC_WebViewController_h

#import <UIKit/UIKit.h>
#import <TSDViOSLib/TSDViOSLib.h>

@interface WebViewController : UIViewController
@property (weak, nonatomic) IBOutlet UIWebView *webView;
@property (strong, nonatomic) JavascriptBridge *jsBridge;

@end

#endif
