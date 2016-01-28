/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include <graphfilter/graphfilter.h>
#include "graphfilterjnilib.h"

JNIEXPORT jboolean JNICALL Java_com_intel_otc_tsdv_GraphFilterJNILib_initNative
  (JNIEnv *env, jobject obj, jstring js_downsamplingSetup, jstring js_dataSchema, jstring js_database_path, jboolean jb)
{
  bool clean = (bool) jb;

  const char *cstr_downsamplingSetup= env->GetStringUTFChars(js_downsamplingSetup, 0);
  std::string downsamplingSetup(cstr_downsamplingSetup);
  env->ReleaseStringUTFChars(js_downsamplingSetup, cstr_downsamplingSetup);

  const char *cstr_dataSchema= env->GetStringUTFChars(js_dataSchema, 0);
  std::string dataSchema(cstr_dataSchema);
  env->ReleaseStringUTFChars(js_dataSchema, cstr_dataSchema);

  const char *cstr_database_path= env->GetStringUTFChars(js_database_path, 0);
  std::string database_path(cstr_database_path);
  env->ReleaseStringUTFChars(js_database_path, cstr_database_path);

  return (jboolean) intel::poc::GraphFilter::instance().init(downsamplingSetup, dataSchema, database_path, clean);
}


JNIEXPORT jboolean JNICALL Java_com_intel_otc_tsdv_GraphFilterJNILib_addDataNative
  (JNIEnv *env, jobject obj, jstring js)
{
  const char *cstr= env->GetStringUTFChars(js, 0);
  std::string data_values(cstr);

  env->ReleaseStringUTFChars(js, cstr);

  return intel::poc::GraphFilter::instance().addData(data_values);
}

JNIEXPORT jstring JNICALL Java_com_intel_otc_tsdv_GraphFilterJNILib_getDataNative
  (JNIEnv *env, jobject obj, jstring js)
{
  const char *cstr= env->GetStringUTFChars(js, 0);
  std::string params(cstr);

  env->ReleaseStringUTFChars(js, cstr);

  std::string result = intel::poc::GraphFilter::instance().getData(params);
  return env->NewStringUTF((const char*) result.c_str());
}
