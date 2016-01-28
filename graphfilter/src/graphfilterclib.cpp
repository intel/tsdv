/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include <graphfilter/graphfilter.h>
#include <graphfilter/graphfilterclib.h>

const char* intel_poc_GraphFilter_id()
{
    std::string id = intel::poc::GraphFilter::instance().id();
    return strdup(id.c_str());
}

int intel_poc_GraphFilter_init(const char* downsampling_setup,
                               const char* data_schema,
                               const char* database_path,
                               int clean)
{
    std::string data_base_path_str(database_path);
    bool is_clean = !!clean;

    return intel::poc::GraphFilter::instance().init(downsampling_setup, data_schema, data_base_path_str,  is_clean);
}

int intel_poc_GraphFilter_addData(const char* data_values)
{
    std::string data_values_str(data_values);

    return intel::poc::GraphFilter::instance().addData(data_values_str);
}

const char* intel_poc_GraphFilter_getData(const char* params)
{
    std::string params_str(params);

    std::string data = intel::poc::GraphFilter::instance().getData(params_str);
    return strdup(data.c_str());
}
