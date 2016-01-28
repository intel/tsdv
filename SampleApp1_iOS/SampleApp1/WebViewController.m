/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#import <JavaScriptCore/JavaScriptCore.h>
#import "WebViewController.h"

@implementation WebViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.webView.delegate = self;
    self.webView.scrollView.bounces = NO;
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"index" withExtension:@"html"];
    [self.webView loadRequest:[NSURLRequest requestWithURL:url]];
}

- (void)webViewDidStartLoad:(UIWebView *)webView {
    NSLog(@"webViewDidStartLoad");
    
	NSString *cacheSetup = @"{\"useCache\": true,"
							"\"cacheRawData\": true,"
							"\"downsamplingFilter\": \"TIME_WEIGHTED_POINTS\","
							"\"fetchAhead\": 1,"
							"\"fetchBehind\": 1,"
							"\"downsamplingLevels\": ["
							"   { \"duration\": 86400,"
							"     \"numOfPoints\": 100"
							"   }"
							" ]"
							"}";
	
	
	NSString *dataSchema = @"{"
                            "    \"table\": \"data\","
                            "    \"date_key_column\": \"date\","
                            "    \"columns\": {"
                            "        \"date\": \"TEXT\","
                            "        \"calories\": \"REAL\","
                            "        \"heart_rate\": \"INT\","
                            "        \"body_temp\": \"REAL\","
                            "        \"steps\": \"INT\","
                            "        \"blood_sugar\": \"REAL\""
                            "    }"
                            "}";

    NSString *databasePath = [[NSBundle mainBundle] pathForResource:@"2MonthData" ofType:@"db"];

    if (databasePath) {
        JSContext *context =  [webView valueForKeyPath:@"documentView.webView.mainFrame.javaScriptContext"];
        self.jsBridge = [[JavascriptBridge alloc] initWithJSContext:context withCacheSetup:cacheSetup withDataSchema:dataSchema withDatabasePath:databasePath];
    } else {
        NSLog(@"Database file not found");
    }
}

- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    NSLog(@"webViewDidFinishLoad");
}

@end