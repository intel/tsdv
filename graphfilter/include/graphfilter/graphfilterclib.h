/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef INTEL_POC_GRAPHFILTERCLIB_H
#define INTEL_POC_GRAPHFILTERCLIB_H

/// C wrapper 
#ifdef __cplusplus
extern "C" {
#endif

    /// str allocated using strdup(), must be freed by caller by calling free();
    const char* intel_poc_GraphFilter_id();

    int intel_poc_GraphFilter_init(const char* downsampling_setup,
                                   const char* data_schema,
                                   const char* database_path,
                                   int clean);


    int intel_poc_GraphFilter_addData(const char* data_values);

    /// str allocated using strdup(), must be freed by caller by calling free();
    const char* intel_poc_GraphFilter_getData(const char* params);

#ifdef __cplusplus
}
#endif

#endif // INTEL_POC_GRAPHFILTERCLIB_H
