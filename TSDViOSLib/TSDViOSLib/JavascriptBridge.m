/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#import "graphfilterclib.h"
#import "JavascriptBridge.h"

@implementation JavascriptBridge

- (void) initLogFile {
    @try {
        // set up log file

        NSDate *date = [NSDate date];
        NSDateFormatter *dateFormat = [[NSDateFormatter alloc] init];
        [dateFormat setDateFormat:@"dd-MMM-yyyy-HH:mm:ss"];
        NSString *dateString = [dateFormat stringFromDate:date];
        NSString *fileString = [@"/iOS-" stringByAppendingString:dateString];
        fileString = [fileString stringByAppendingString:@".txt"];

        NSArray *dirs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentDir = [dirs objectAtIndex:0];
        self.logFilePath = [documentDir stringByAppendingString:fileString];
        self.logFileHandle = [NSFileHandle fileHandleForUpdatingAtPath:self.logFilePath];
        NSString *logStr = [NSString stringWithFormat:@"timestamp,duration,datasize,method\n"];
        NSError *error = nil;
        [logStr writeToFile:self.logFilePath atomically:YES encoding:NSUTF8StringEncoding error:&error];
    }
    @catch (NSException *exception) {

    }
}

- (void)deleteOldLogs {
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"^iOS.*.txt$"
                                                                           options:NSRegularExpressionCaseInsensitive
                                                                             error:nil];

    NSArray *dirs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentDir = [dirs objectAtIndex:0];

    NSDirectoryEnumerator *filesEnumerator = [[NSFileManager defaultManager] enumeratorAtPath:documentDir];

    NSString *file;
    NSError *error;
    while (file = [filesEnumerator nextObject]) {
        NSUInteger match = [regex numberOfMatchesInString:file
                                                  options:0
                                                    range:NSMakeRange(0, [file length])];
        NSString *matchPath = [documentDir stringByAppendingString:[@"/" stringByAppendingString:file]];
        if (match && ![matchPath isEqualToString:self.logFilePath]) {
            [[NSFileManager defaultManager] removeItemAtPath:[documentDir stringByAppendingPathComponent:file] error:&error];
        }
    }
}

- (id)initWithJSContext:(JSContext *)jsContext withCacheSetup:(NSString *)cacheSetup withDataSchema:(NSString *)dataSchema withDatabasePath:(NSString*)databasePath {
    self = [super init];
    if (self) {
        self.jsContext = jsContext;

        // wrapper for console.log() so we can debug messages using NSLog
        [jsContext evaluateScript:@"var console = {};"];

        jsContext[@"console"][@"log"] = ^(NSString *message)
        {
            NSLog(@"Console: %@", message);
        };

        // wrapper for TSDV object and maps javascript func to ObjectiveC
        [jsContext evaluateScript:@"var TSDV = {};"];

        jsContext[@"TSDV"][@"loadData"] = ^(NSString *params,
                                            NSString *callback,
                                            NSString *direction,
                                            NSString *errorCallback) {
            @try {
                NSString* result = [self getData:params];
                JSValue* jsFunction = jsContext[callback];
                NSArray* jsArgs = @[result, direction];
                [jsFunction callWithArguments:jsArgs];
            }
            @catch (NSException *exception) {
                NSString* errorMsg = @"Exception caught in loadData";
                JSValue* jsFunction = jsContext[errorCallback];
                [jsFunction callWithArguments:@[errorMsg]];
            }
        };

        // returns true if logging is enabled
        jsContext[@"TSDV"][@"loggingOptionEnabled"] = (NSNumber*) ^(NSString *option){
            return [self loggingOptionEnabled:option];
        };

        // sets if logging is enabled
        jsContext[@"TSDV"][@"setLoggingOption"] = (NSString*) ^(NSString *option,
                                                                bool enable) {
            return [self setLoggingOption:option :enable];
        };

        // remove old log files
        jsContext[@"TSDV"][@"deleteOldLogs"] = (NSNumber*) ^(){
            return [self deleteOldLogs];
        };

        // maps javascript func to ObjectiveC
        jsContext[@"TSDV"][@"logToFile"] = (NSString*) ^(NSString *timeStamp,
                                                         int renderTime,
                                                         int dataSize,
                                                         NSString *callFunc) {
            return [self logToFile:timeStamp :renderTime :dataSize :callFunc];
        };

        // emits signal from javascript
        jsContext[@"TSDV"][@"emitSignal"] = (NSString*) ^(NSString *signalName,
                                                          NSString *values) {
            return [self emitSignal:signalName :values];
        };

        NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
        if ([userDefaults boolForKey:@"LOGGING_ENABLED"])
            [self initLogFile];

        if (!intel_poc_GraphFilter_init([cacheSetup UTF8String], [dataSchema UTF8String], [databasePath UTF8String], NO)) {
            NSLog(@"Cannot initialize graph filter");
            return self;
        }
    }

    return self;
}

- (NSString*)getData:(NSString *)params {
    @try {
        NSError *error = nil;
        NSData *paramsData = [params dataUsingEncoding:NSUTF8StringEncoding];
        NSDictionary *dict = [NSJSONSerialization JSONObjectWithData:paramsData options:NSJSONReadingMutableLeaves error:&error];

        if (!dict) {
            NSLog(@"Failed to parse params");
            return @"[]";
        }

        NSString *startDateString = [dict objectForKey:@"startDate"];
        NSString *endDateString = [dict objectForKey:@"endDate"];
        
        if ([startDateString length] == 0 || [endDateString length] == 0) {
            NSLog(@"No startDate or endDate provided");
            return @"[]";
        }

        const char *result = intel_poc_GraphFilter_getData([params UTF8String]);
        NSString *resultString = [NSString stringWithUTF8String:result];
        free((char*) result);

        if ([resultString length] != 0) {
            NSData *resultsData = [resultString dataUsingEncoding:NSUTF8StringEncoding];
            NSArray *array= [NSJSONSerialization JSONObjectWithData:resultsData options:NSJSONReadingMutableLeaves error:&error];
            if (!array) {
                NSLog(@"Failed to parse result");
                return @"[]";
            }

            return resultString;
        } else {
            return @"[]";
        }
    }
    @catch (NSException *exception) {
        NSLog(@"Exception: %@", [exception reason]);
        return @"[]";
    }
}

- (NSNumber*)loggingOptionEnabled:(NSString*)option {
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
    NSNumber *returnVal = [userDefaults valueForKey:option];
    if (returnVal == nil)
        returnVal = 0;
    return returnVal;
}

- (void)setLoggingOption:(NSString*)option :(bool)enable {
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
    [userDefaults setBool:(enable) forKey:option];

    //If log file has not been created yet, create it.
    if ([option isEqualToString:@"LOGGING_ENABLED"] && enable && self.logFilePath == NULL)
    {
        [self initLogFile];
    }
}

- (void)logToFile:(NSString*)timeStamp :(int)renderTime :(int)dataSize :(NSString*)callFunc {
    @try {
        NSString *logStr = [NSString stringWithFormat:@"%@,%d,%d,%@\n", timeStamp, renderTime, dataSize, callFunc];
        self.logFileHandle = [NSFileHandle fileHandleForUpdatingAtPath:self.logFilePath];
        [self.logFileHandle seekToEndOfFile];
        [self.logFileHandle writeData:[logStr dataUsingEncoding:NSUTF8StringEncoding]];
    }
    @catch (NSException *exception) {
        return;
    }
    return;
}

- (void)emitSignal:(NSString*)signalName :(NSString*)values {
    if (self.signalListener != NULL)
    {
        [self.signalListener onSignalReceived:signalName :values];
    } 
    @try {
        if ([self loggingOptionEnabled:@"LOGGING_ENABLED"])
        {
            [self logToFile:signalName :0 :0 :values];
        }
    }
    @catch (NSException *exception) {
        return;
    }
}

@end
