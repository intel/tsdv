/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.	The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include <graphfilter/databasegraphfilter.h>
#include <graphfilter/sqlitedatacache.h>
#include <graphfilter/sqlitedatabaseaccess.h>
#include <algorithm>
#include <stdexcept>
#include "json.h"

using namespace std;

#ifdef ANDROID
#include <android/log.h>
#define  LOG_TAG    "DatabaseGraphFilter"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI(...)  printf(__VA_ARGS__)
#define  LOGD(...)  printf(__VA_ARGS__)
#define  LOGE(...)  printf(__VA_ARGS__)
#endif

namespace intel {
    namespace poc {

        DatabaseGraphFilter::DatabaseGraphFilter()
            : GraphFilter()
        {
            initialized_ = false;
        }

        DatabaseGraphFilter::~DatabaseGraphFilter()
        {
        }

        const std::string DatabaseGraphFilter::id() const
        {
            return std::string("SQLite3");
        }

        bool DatabaseGraphFilter::init(const std::string& cache_setup,
                                       const std::string& data_schema,
                                       const std::string& database_path,
                                       bool clean)
        {
            initialized_ = false;
            use_cache_ = false;
            cache_raw_data_ = false;
            downsampling_filter_ = DataFilter::FilterType::TIME_WEIGHTED_POINTS;

            Json::Reader reader;

            // Parse cache_setup
            Json::Value cache_setup_json;
            if (cache_setup.empty()){
                use_cache_ = false;
            } else if(reader.parse(cache_setup, cache_setup_json)) {
                use_cache_ = cache_setup_json.isMember("useCache") ? cache_setup_json["useCache"].asBool() : false;
                cache_raw_data_ = cache_setup_json.isMember("cacheRawData") ? cache_setup_json["cacheRawData"].asBool() : false;
                downsampling_filter_ = cache_setup_json.isMember("downsamplingFilter") ? DataFilter::getType(cache_setup_json["downsamplingFilter"].asString()) : DataFilter::FilterType::TIME_WEIGHTED_POINTS;
            } else {
                LOGE("Cannot parse cache setup param: %s\n", cache_setup.c_str());
                return false;
            }

            if(use_cache_){
                LOGD("Using cache\n");
            }


            // Parse data_schema
            Json::Value data_schema_json;
            if (reader.parse(data_schema, data_schema_json)) {
                if(!data_schema_json.isMember("table") || !data_schema_json.isMember("date_key_column") || !data_schema_json.isMember("columns")){
                    LOGE("Malformed data schema value: %s\n", data_schema.c_str());
                    return false;
                } else {
                    std::vector<std::string> data_names = data_schema_json["columns"].getMemberNames();
                    if(std::find(data_names.begin(),data_names.end(),data_schema_json["date_key_column"].asString()) == data_names.end()){
                        LOGE("Date key column not found in column list\n");
                        return false;
                    }
                    for(std::vector<std::string>::iterator it = data_names.begin(); it != data_names.end(); ++it){
                        data_schema_[*it] = data_schema_json["columns"][*it].asString();
                    }
                }
            } else {
                LOGE("Cannot parse data schema param: %s\n", data_schema.c_str());
                return false;
            }

            // Initialize the cache, database, and data filter
            if(use_cache_ && !SQLiteDataCache::instance().init(cache_setup_json, data_schema_json, true)){
                LOGE("Cannot initialize cache\n");
                return false;
            }
            if(!SQLiteDatabaseAccess::instance().init(database_path, data_schema_json, clean)){
                LOGD("Cannot initalize database\n");
                return false;
            }
            if(!DataFilter::init(data_schema_json["date_key_column"].asString())){
                LOGD("Cannot initalize data filter\n");
                return false;
            }

            initialized_ = true;

            return true;
        }

        bool DatabaseGraphFilter::addData(const std::string& data_values)
        {
            if(!initialized_) {
                LOGE("Error: Database not initialized\n");
                return false;
            }

            Json::Reader reader;
            Json::Value data_values_json;
            if(!reader.parse(data_values, data_values_json)) {
                LOGE("Unable to parse json data: %s\n", data_values.c_str());
                return false;
            }

            return SQLiteDatabaseAccess::instance().putData(data_values_json);
        }

        std::string DatabaseGraphFilter::getData(const std::string& params)
        {
            Json::FastWriter fastWriter;
            Json::Value empty_response;
            empty_response["startDate"] = "";
            empty_response["endDate"] = "";
            empty_response["points"] = Json::Value(Json::arrayValue);

            if (!initialized_) {
                LOGE("Error: Database not initialized\n");
                return fastWriter.write(empty_response);
            }

            // Parse the query params
            Json::Value params_json;
            Json::Reader reader;
            if(!reader.parse(params, params_json)) {
                LOGE("Unable to parse json data: %s\n", params.c_str());
                return fastWriter.write(empty_response);
            }
            if(!params_json.isMember("startDate") || !params_json.isMember("endDate")){
                LOGE("Invalid query params: %s\n", params.c_str());
                return fastWriter.write(empty_response);
            }
            std::string start_date = params_json["startDate"].asString();
            std::string end_date = params_json["endDate"].asString();
            int num_of_points = params_json.get("numOfPoints", 0).asInt();

            if(use_cache_){
                try{
                    SQLiteDataCache::instance().cacheData(start_date, end_date);
                } catch (std::exception& ex) {
                    LOGE("Failed caching data: %s\n", ex.what());
                }
            }

            if(num_of_points <= 0) {
                LOGD("Requested 0 results. Returning empty-point response.\n");
                Json::Value response = empty_response;
                response["startDate"] = start_date;
                response["endDate"] = end_date;
                return fastWriter.write(response);
            }


            // Check the cache and parse its results
            Json::Value json_response = use_cache_ ? SQLiteDataCache::instance().getData(params_json) : Json::Value(Json::objectValue);
            std::string cache_start_date = json_response.get("startDate", "").asString();
            std::string cache_end_date = json_response.get("endDate", "").asString();

            // If needed, pull data from database and parse its results
            if(cache_start_date != start_date || cache_end_date != end_date) {
                json_response = SQLiteDatabaseAccess::instance().getData(params_json);
            } else if(!cache_raw_data_ && json_response.isMember("points") && json_response["points"].isArray()){
                // If we're not caching raw data and the cache returns a valid response, we're done.    If
                // we ARE caching raw data, we might still need to downsample.
                return fastWriter.write(json_response);
            }

            if(!json_response.isMember("points") || !json_response["points"].isArray()){
                LOGD("Database and/or cache response missing points array. Returning empty response.\n");
                return fastWriter.write(empty_response);
            }

            // Downsample data if needed.
            if(json_response["points"].size() > num_of_points) {
                LOGD("Downsample data to total %d points of data\n", num_of_points);
                json_response = DataFilter::applyFilter(json_response, data_schema_, num_of_points, downsampling_filter_);
            }


            return fastWriter.write(json_response);
        }

    }
}
