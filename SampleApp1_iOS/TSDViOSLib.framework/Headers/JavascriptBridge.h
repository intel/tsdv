/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef JavascriptBridge_h
#define JavascriptBridge_h

#import <JavaScriptCore/JavaScriptCore.h>

@interface TSDVSignalListener : NSObject

- (void)onSignalReceived:(NSString*)signalName :(NSString*)values;

@end

@interface JavascriptBridge : NSObject

@property (strong, nonatomic) JSContext *jsContext;

@property NSString *logFilePath;

@property NSFileHandle *logFileHandle;

@property TSDVSignalListener *signalListener;

- (void)initLogFile;

- (void)deleteOldLogs;

- (id)initWithJSContext:(JSContext *)jsContext withCacheSetup:(NSString *)cacheSetup withDataSchema:(NSString *)dataSchema withDatabasePath:(NSString *)databasePath;

- (NSString*)getData:(NSString *)params;

- (NSNumber*)loggingOptionEnabled:(NSString*)option;

- (void)setLoggingOption:(NSString*)option :(bool)enable;

- (void)logToFile:(NSString*)timeStamp :(int)renderTime :(int)dataSize :(NSString*)callFunc;

- (void)setSignalListener:(TSDVSignalListener*)listener;

- (void)emitSignal:(NSString*)signalName :(NSString*)values;
@end

#endif
