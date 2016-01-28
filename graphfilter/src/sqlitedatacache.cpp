/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifdef ANDROID
#include <android/log.h>
#define  LOG_TAG    "SQLiteDataCache"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI(...)  printf(__VA_ARGS__)
#define  LOGD(...)  printf(__VA_ARGS__)
#define  LOGE(...)  printf(__VA_ARGS__)
#endif


#include <graphfilter/sqlitedatacache.h>
#include <graphfilter/sqlitedatabaseaccess.h>
#include <stdexcept>
#include <stdlib.h>
#include <algorithm>
#include <thread>
#include <future>
#include <time.h>

namespace intel { namespace poc {

    DataCache& SQLiteDataCache::instance()
    {
        static DataCache *instance = new SQLiteDataCache();

        return *instance;
    }

    SQLiteDataCache::~SQLiteDataCache()
    {
        if (database_) {
            sqlite3_close(database_);
        }
    }

    const std::string SQLiteDataCache::database_path_ = ":memory:";

    bool SQLiteDataCache::init(const Json::Value& cache_setup, const Json::Value& data_schema, bool clean){
        // Wait until any putData calls are done in case someone re-calls init after putData.
        std::lock_guard<std::mutex> guard(put_data_mutex_);

        initialized_ = false;
        cache_data_bounds_.clear();
        cache_levels_.clear();

        // Parse cache_setup
        if(!cache_setup.isObject()){
            LOGE("cache_setup not object json type: %s", cache_setup.toStyledString().c_str());
            return false;
        }
        cache_raw_data_ = cache_setup.isMember("cacheRawData") ? cache_setup["cacheRawData"].asBool() : false;
        fetch_ahead_ = cache_setup.isMember("fetchAhead") ? cache_setup["fetchAhead"].asInt() : 0;
        fetch_behind_ = cache_setup.isMember("fetchBehind") ? cache_setup["fetchBehind"].asInt() : 0;
        downsampling_filter_ = cache_setup.isMember("downsamplingFilter") ? DataFilter::getType(cache_setup["downsamplingFilter"].asString()) : DataFilter::FilterType::TIME_WEIGHTED_POINTS;
        if(cache_setup.isMember("downsamplingLevels") && cache_setup["downsamplingLevels"].isArray()){
            //LOGD("Requested downsampling levels: %s\n",cache_setup["downsamplingLevels"].toStyledString().c_str());
            for(int i=0; i < cache_setup["downsamplingLevels"].size(); ++i){
                if(!cache_setup["downsamplingLevels"][i].isMember("duration") || !cache_setup["downsamplingLevels"][i].isMember("numOfPoints")){
                    LOGE("Bad downsamplingLevels value found; skipping.\n");
                    continue;
                } else {
                    std::map<std::string,long> level;
                    level["duration"] = static_cast<long>(cache_setup["downsamplingLevels"][i]["duration"].asLargestInt());
                    level["num_of_points"] = static_cast<long>(cache_setup["downsamplingLevels"][i]["numOfPoints"].asLargestInt());
                    cache_levels_.push_back(level);
                }
            }
        }

        // Parse data_schema
        if(!data_schema.isObject()){
            LOGE("data_schema not object json type: %s", data_schema.toStyledString().c_str());
            return false;
        }
        if(!data_schema.isMember("table") || !data_schema.isMember("date_key_column") || !data_schema.isMember("columns")){
            LOGE("Malformed data schema value: %s\n", data_schema.toStyledString().c_str());
            return false;
        } else {
            table_name_ = data_schema.get("table", "table").asString() + "_cache";

            date_key_column_ = data_schema["date_key_column"].asString();
            std::vector<std::string> data_names = data_schema["columns"].getMemberNames();
            if(std::find(data_names.begin(),data_names.end(),date_key_column_) == data_names.end()){
                LOGE("Date key column not found in column list\n");
                return false;
            }
            for(std::vector<std::string>::iterator it = data_names.begin(); it != data_names.end(); ++it){
                data_schema_[*it] = data_schema["columns"][*it].asString();
            }
        }

        if(!openDatabase()){
            LOGE("Failed to open database: %s\n", database_path_.c_str());
            return false;
        }

        // In memory databases are always erased, so they are ALWAYS clean and MUST be created
        // fresh each time.
        if(!clean){
            LOGE("Param 'clean' = false passed. SQLiteDataCache does not support persisted data cache.\n");
            return false;
        }
        try{
            createDatabase();
        }  catch (std::exception& ex) {
            LOGE("Failed to create cache database: %s\n", ex.what());
            return false;
        }

        initialized_ = true;

        return initialized_;
    }

    void SQLiteDataCache::cacheData(const std::string& start_date, const std::string& end_date){
        if (!initialized_) {
            LOGE("Error: Database not initialized\n");
            throw std::runtime_error(std::string("Database not yet initialized."));
        }

        if(start_date.empty() || end_date.empty() || timeStringToEpochSeconds(start_date) <= 0 || timeStringToEpochSeconds(end_date) <= 0){
            throw std::runtime_error(std::string("Invalid start_date or end_date: ") + start_date + ", " + end_date);
        }

        LOGD("Calling cacheData in separate thread.\n");
        std::thread (&SQLiteDataCache::cacheDataAsync, this, start_date, end_date).detach();
    }

    bool SQLiteDataCache::getAndPutData(const std::string& start_date, const std::string& end_date){
        LOGD("Adding portion of data to cache from %s to %s\n", start_date.c_str(), end_date.c_str());
        Json::Value params_json;
        params_json["startDate"] = start_date;
        params_json["endDate"] = end_date;
        Json::Value data_values = SQLiteDatabaseAccess::instance().getData(params_json);

        if (!data_values.isMember("startDate") || !data_values.isMember("endDate") || !data_values.isMember("points")){
            LOGE("Invalid data: startDate, endDate, or points missing.\n");
            return false;
        }

        if(!data_values["points"].isArray() || data_values["points"].size() == 0){
            LOGD("No points to put into databasae.\n");
            return true;
        }

        // Add the data to the database
        bool putDataSuccess = true;

        std::vector<std::future<bool>> results;

        if(cache_raw_data_){
            results.push_back(std::async(std::launch::async, &SQLiteDataCache::putDataTable, this, table_name_ + "_raw", data_values["points"]));
        }

        // Start all async puts
        for(int level=1; level <= cache_levels_.size(); ++level){
            results.push_back(std::async(std::launch::async, &SQLiteDataCache::downsampleAndPutData, this, level, data_values));
        }
        // Retrieve all success values
        for(int i=0; i < results.size(); ++i){
            putDataSuccess = putDataSuccess && results[i].get();
        }

        return putDataSuccess;
    }

    bool SQLiteDataCache::downsampleAndPutData(int level, const Json::Value& data_values){
        bool putDataSuccess = true;
        std::stringstream buff;
        buff << "_" << level;
        std::string start_date = data_values.get("startDate", "").asString();
        std::string end_date = data_values.get("endDate", "").asString();
        //putDataSuccess = putDataSuccess && clearDatabaseRange(table_name_ + buff.str(), start_date, end_date);
        Json::Value json_response = DataFilter::applyFilter(data_values, data_schema_, getDurationNumPoints(start_date, end_date, level),downsampling_filter_);
        putDataSuccess = putDataSuccess && putDataTable(table_name_ + buff.str(), json_response["points"]);
        return putDataSuccess;
    }

    void SQLiteDataCache::cacheDataAsync(const std::string& start_date, const std::string& end_date){
        std::lock_guard<std::mutex> guard(put_data_mutex_);

        std::string start_date_cache = start_date;
        std::string end_date_cache = end_date;

        long duration = timeStringToEpochSeconds(end_date_cache) - timeStringToEpochSeconds(start_date_cache);
        if(fetch_behind_ > 0) {
            start_date_cache = updateTimeString(start_date_cache, -1 * fetch_behind_ * duration);
        }
        if(fetch_ahead_ > 0){
            end_date_cache = updateTimeString(end_date_cache, fetch_ahead_ * duration);
        }

        std::string start_date_tmp = start_date_cache;
        std::string end_date_tmp = end_date_cache;


        LOGD("Adding data to cache from %s to %s\n", start_date_cache.c_str(), end_date_cache.c_str());

        // Create a copy of the current state so we aren't affecting the original yet
        std::unique_lock<std::mutex> lock_read(cache_data_bounds_mutex_);
        std::map<std::string,std::string> cache_data_bounds_tmp = cache_data_bounds_;
        lock_read.unlock();
        // Check the data cache bounds against this query
        std::map<std::string,std::string>::iterator it = cache_data_bounds_tmp.begin();
        while( it != cache_data_bounds_tmp.end()){
            if(start_date_tmp.compare(it->first) >= 0 && end_date_tmp.compare(it->second) <= 0){
                // Cache already contains all data for this range
                LOGD("Cache already contains all data.\n");
                return;
            } else if(start_date_tmp.compare(it->second) > 0 || end_date_tmp.compare(it->first) < 0){
                // No conflict with this set, keep looking
                ++it;
            } else if((start_date_tmp.compare(it->first) <= 0 && end_date_tmp.compare(it->second) <= 0) || end_date_tmp.compare(it->first) == 0){
                // New data overlaps data to the left. Use new start date as start bound and old end date as end bound
                LOGD("New data overlaps cache data to the left.\n");
                end_date_tmp = it->second;
                // Remove the old value, as new one will replace it
                cache_data_bounds_tmp.erase(it++);  // NOTE: post-increment is VERY IMPORTANT here
            } else if((start_date_tmp.compare(it->first) >= 0 && end_date_tmp.compare(it->second) >= 0) || start_date_tmp.compare(it->second) == 0){
                 // New data overlaps data to the right. Use old start date as start bound and new end date as end bound
                 LOGD("New data overlaps cache data to the right.\n");
                 start_date_tmp = it->first;
                 // Remove the old value, as new one will replace it
                 cache_data_bounds_tmp.erase(it++);  // NOTE: post-increment is VERY IMPORTANT here
             } else if(start_date_tmp.compare(it->first) <= 0 && end_date_tmp.compare(it->second) >= 0){
                 // Old data is a subset of new data. Use new start and end date
                 LOGD("Old data is a subset of new data.\n");
                 // remove the old value, as new one will replace it
                 cache_data_bounds_tmp.erase(it++);  // NOTE: post-increment is VERY IMPORTANT here
             } else {
                ++it;
            }
        }

        // Put the new value in the cache data bounds map
        cache_data_bounds_tmp[start_date_tmp] = end_date_tmp;


        // Add the data to the database
        bool putDataSuccess = true;

        std::vector<std::future<bool>> results;


        std::map<std::string,std::string> cacheDiff = getCacheDifference(start_date_cache, end_date_cache);

        for(std::map<std::string,std::string>::iterator it = cacheDiff.begin(); it != cacheDiff.end(); ++it){
            results.push_back(std::async(std::launch::async, &SQLiteDataCache::getAndPutData, this, it->first, it->second));
        }

        // Retrieve all success values
        for(int i=0; i < results.size(); ++i){
            putDataSuccess = putDataSuccess && results[i].get();
        }

        if(putDataSuccess){
            // We successfully added the data to the cache, so update the bounds
            std::unique_lock<std::mutex> lock_write(cache_data_bounds_mutex_);
            cache_data_bounds_ = cache_data_bounds_tmp;
            lock_write.unlock();
        }
    }



    bool SQLiteDataCache::putDataTable(const std::string& table_name, const Json::Value& points){
        // Build the SQL insert string
        std::string query = "BEGIN TRANSACTION; "
            "INSERT OR IGNORE INTO " + table_name + " (";

        for(std::map<std::string,std::string>::iterator it = data_schema_.begin(); it != data_schema_.end(); ++it){
            if(it != data_schema_.begin()){
                query += ", ";
            }
            query += it->first;
        }

        query += ") VALUES ";
        for (int i = 0; i < points.size(); ++i ){
            Json::Value data_point = points[i];

            if(!data_point.isMember(date_key_column_)){
                LOGE("Invalid data point encountered. Skipping.");
                continue;
            }

            query += "(";

            for(std::map<std::string,std::string>::iterator it = data_schema_.begin(); it != data_schema_.end(); ++it){
                if(it != data_schema_.begin()){
                    query += ", ";
                }
                if(data_point.isMember(it->first)){
                    query += "'" + data_point[it->first].asString() + "'";
                } else {
                    query += "NULL";
                }
            }

            if(i > 0 && i % 499 == 0){
                query += "); INSERT OR IGNORE INTO " + table_name + " (";
                for(std::map<std::string,std::string>::iterator it = data_schema_.begin(); it != data_schema_.end(); ++it){
                    if(it != data_schema_.begin()){
                        query += ", ";
                    }
                    query += it->first;
                }
                query += ") VALUES ";
            } else if(i != points.size() - 1){
                query += "),";
            }  else {
                query += ");";
            }
        }
        query += "COMMIT TRANSACTION;";

        try {
            LOGD("Adding %d data points to cache table %s\n",points.size(), table_name.c_str());
            executeQuery(query);
            return true;
        } catch (std::exception& ex) {
            LOGE("Exceptions caught: %s\n", ex.what());
            // Try to roll back the transaction that is likely left hanging
            try {
                LOGE("Attempting to roll back transaction.\n");
                executeQuery("ROLLBACK TRANSACTION;");
            } catch (std::exception& ex) {
                LOGE("Exception caught trying to roll back transaction: %s\n", ex.what());
            }
            return false;
        }
    }

    Json::Value SQLiteDataCache::getData(const Json::Value& params){
        Json::Value empty_response;
        empty_response["startDate"] = "";
        empty_response["endDate"] = "";
        empty_response["points"] = Json::Value(Json::arrayValue);
        //empty_response["points"] = Json::arrayValue;

        if (!initialized_) {
            LOGE("Error: Database not initialized\n");
            return empty_response;
        }

        // Parse the query params
        if(!params.isObject()){
            LOGE("params not object json type: %s", params.toStyledString().c_str());
            return empty_response;
        }
        if(!params.isMember("startDate") || !params.isMember("endDate")){
            LOGE("Invalid query params: %s\n", params.toStyledString().c_str());
            return empty_response;
        }
        std::string query_start_time = params["startDate"].asString();
        std::string query_end_time = params["endDate"].asString();
        int num_of_points = params.get("numOfPoints", 0).asInt();
        const Json::Value metrics = params["metrics"];

        if(num_of_points <= 0){
            LOGD("Requesting <= 0 points.");
            Json::Value response = empty_response;
            response["startDate"] = query_start_time;
            response["endDate"] = query_end_time;
            return response;
        }

        std::string table_name = cacheContains(query_start_time, query_end_time, num_of_points);
        if(table_name == ""){
            return empty_response;
        }

        std::vector<std::string> json_fields;

        //LOGD("Metrics: %s\n",metrics.toStyledString().c_str());
        if(metrics.empty() || (metrics.size() == 1 && metrics[0].asString() == "*")){
            // Include all metrics
            //LOGD("Including all metrics\n");
            for(std::map<std::string,std::string>::iterator it = data_schema_.begin(); it != data_schema_.end(); ++it){
                json_fields.push_back(std::string(it->first));
            }

        } else {
            // Parse through metrics to build vectors
            json_fields.push_back(date_key_column_);
            std::map<std::string,std::string>::iterator it;
            for (int i = 0; i < metrics.size(); i++ ) {
                // Check if metric is valid
                it = data_schema_.find(metrics[i].asString());
                if(it == data_schema_.end()){
                    LOGD("Invalid metric found: %s\n",metrics[i].asString().c_str());
                    return empty_response;
                }
                json_fields.push_back(metrics[i].asString());
            }
        }

        // build the query columns string
        std::string columns = "";
        for(std::vector<std::string>::iterator it = json_fields.begin(); it != json_fields.end(); ++it){
            if(it == json_fields.begin()){
                columns = *it;
            } else {
                columns += ", " + *it;
            }
        }

        // Build the SQL query
        std::stringstream query;
        query << "SELECT ";
        query << columns;
        query << " FROM " + table_name;
        query << " WHERE " << date_key_column_ << " BETWEEN \"" << query_start_time << "\" AND \"" << query_end_time << "\" ";
        query << " ORDER BY " << date_key_column_ << " ASC;";

        int num_of_fields = static_cast<int>(json_fields.size());
        std::string sql_query = query.str();

        Json::Value response = empty_response;
        response["startDate"] = query_start_time;
        response["endDate"] = query_end_time;
        try{
            sqlite3_stmt *stmt;

            //LOGD("Executing SQL query %s: \n", sql_query.c_str());
            int rc = sqlite3_prepare_v2(database_, sql_query.c_str(), -1, &stmt, NULL);

            if (rc != SQLITE_OK) {
                std::string err_msg(sqlite3_errmsg(database_));
                LOGE("Error processing SQL query: %s\n", sql_query.c_str());
                return empty_response;
            } else if(sqlite3_column_count(stmt) != num_of_fields){
                LOGE("Number of returned columns does not match number expected.");
                return empty_response;
            }

            // Build response object from query response
            while (sqlite3_step(stmt) == SQLITE_ROW) {

                // Begin constructing json object
                Json::Value point(Json::objectValue); // {}

                for(int i=0; i < num_of_fields; ++i){
                    std::string type = data_schema_[json_fields[i]];
                    if(type == "INT"){
                        point[json_fields[i]] = Json::Value(Json::intValue);
                        point[json_fields[i]] = sqlite3_column_int(stmt, i);
                    } else if (type == "REAL"){
                        point[json_fields[i]] = Json::Value(Json::realValue );
                        point[json_fields[i]] = sqlite3_column_double(stmt, i);
                    } else if (type == "TEXT"){
                        point[json_fields[i]] = Json::Value(Json::stringValue  );
                        point[json_fields[i]] = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)));
                    }
                }
                response["points"].append(point);
            }
        } catch (std::exception& ex) {
            LOGE("Exceptions caught: %s\n", ex.what());
            return empty_response;
        }
        return response;
    }

    /// private API

    /**
    * Checks whether or not the cache contains data for the given dates and for the given number of points.
    *
    * @param[in] start_date timestampof the format: "YYYY-MM-DD HH:MMZ"
    * @param[in] end_date timestampof the format: "YYYY-MM-DD HH:MMZ"
    * @param[in] num_of_points the number of points requested
    *
    * @retval "<table_name>" The name of the table that will satisfy the request
    * @retval "" Failed to find a cache level that will satisfy the request
    */
    std::string SQLiteDataCache::cacheContains(const std::string& start_date, const std::string& end_date, int num_of_points){
        LOGD("Checking cache: start_date = %s, end_date = %s\n",start_date.c_str(),end_date.c_str());
        std::unique_lock<std::mutex> lock_read(cache_data_bounds_mutex_);
        for(std::map<std::string,std::string>::iterator it = cache_data_bounds_.begin(); it != cache_data_bounds_.end(); ++it){
            LOGD("Comparing against cache: cacheStart = %s, cacheEnd = %s\n",it->first.c_str(),it->second.c_str());
            if(start_date.compare(it->first) >= 0 && end_date.compare(it->second) <= 0){
                LOGD("Data is in cache. Checking requested points.\n");



                for(int level=1; level <= cache_levels_.size(); ++level){
                    long put_duration = timeStringToEpochSeconds(end_date.c_str()) - timeStringToEpochSeconds(start_date.c_str());
                    long level_duration = cache_levels_[level-1]["duration"];
                    long level_points = cache_levels_[level-1]["num_of_points"];

                    /*
                    LOGD("chache level %d has %ld points for every %ld sec\n",level,level_points,level_duration);
                    LOGD("put_duration %ld / level_duration %ld = %f durations",put_duration,level_duration,static_cast<double>(put_duration)/static_cast<double>(level_duration));
                    LOGD(" = %f points for put duration = %d rounded points\n", static_cast<double>(put_duration)/static_cast<double>(level_duration)*level_points, static_cast<int>(static_cast<double>(put_duration)/static_cast<double>(level_duration)*level_points));
                    LOGD("Request wants %d points.\n",num_of_points);
                    */

                    if(static_cast<int>(static_cast<double>(put_duration)/static_cast<double>(level_duration)*level_points) == num_of_points){
                        std::stringstream buff;
                        buff << "_" << level;
                        std::string table_name = table_name_ + buff.str();
                        LOGD("Cache table %s will satisfy request.\n", table_name.c_str());
                        return table_name;
                    }
                }

                if(cache_raw_data_){
                    LOGD("Falling back to cached raw data.\n");
                    return table_name_ + "_raw";
                } else {
                    LOGD("No cache level found to satisfy request.\n");
                    return "";
                }
            }
        }
        lock_read.unlock();
        LOGD("Data not found in cache.\n");
        return "";
    }

    /**
    * Returns the difference between your search interval and the cache. That is, returns a map
    *              of <start_date,end_date> pairs representing the intervals in your search range
    *              not already present in the cache.
    *
    * Note that this function relies on the guaranteed sorted ordering of keys in std::map.
    *
    * @param[in] start_date timestampof the format: "YYYY-MM-DD HH:MMZ"
    * @param[in] end_date timestampof the format: "YYYY-MM-DD HH:MMZ"
    *
    * @return The map containing the difference between your search interval and the cache. If
    *  an empty map is returned, the cache already contains your entire search interval.
    */
    std::map<std::string,std::string> SQLiteDataCache::getCacheDifference(std::string start_date, std::string end_date){
        std::map<std::string,std::string> result;
        for(std::map<std::string,std::string>::iterator it = cache_data_bounds_.begin(); it != cache_data_bounds_.end(); ++it){
            if(it->second.compare(start_date) <= 0 ){
                // Skip if this cache interval ends before our start_date
                continue;
            } else if(it->first.compare(end_date) >= 0){
                // If this cache interval starts after our end_date, stop iterating
                break;
            } else if(it->first.compare(start_date) <= 0 && it->second.compare(end_date) >= 0) {
                // If this cache interval completely overlaps the search, return empty set
                return result;
            } else if(it->first.compare(start_date) <= 0 && it->second.compare(end_date) < 0) {
                // If this cache interval overlaps to the left, remove overlapping portion from the search
                start_date = it->second;
            } else if(it->first.compare(start_date) > 0 && it->second.compare(end_date) < 0) {
                // If the search completely overlaps this cache interval, add the left hand overlap
                // and update the search.
                result[start_date] = it->first;
                start_date = it->second;
            } else if(it->first.compare(start_date) > 0 && it->second.compare(end_date) >= 0) {
                // If this cache interval overlaps to the right, add the left hand overlap
                // and return
                result[start_date] = it->first;
                return result;
            }
        }
        // If we didn't return mid-loop, add the current search and return
        result[start_date] = end_date;
        return result;
    }

    long SQLiteDataCache::timeStringToEpochSeconds(const std::string& time_string){
        struct tm tm = {};
        // 2015-03-03 00:00Z
        // TODO: Expand this to check for and allow other common date formats
        if (strptime(time_string.c_str(), "%Y-%m-%d %H:%MZ", &tm) == NULL){
            LOGE("Error converting time_string to time struct: %s\n", time_string.c_str());
            return -1;
        }
        return static_cast<long>(mktime(&tm));
    }

    std::string SQLiteDataCache::updateTimeString(const std::string& time_string, long offset){
        std::time_t time = static_cast<time_t>(timeStringToEpochSeconds(time_string) + offset);
        char buffer [30];
        strftime (buffer,30,"%Y-%m-%d %H:%MZ",localtime(&time));
        //LOGD("Original time %s + %ld sec = %s\n", time_string.c_str(), offset, buffer);
        return std::string(buffer);
    }

    long SQLiteDataCache::getDurationNumPoints(const std::string& start_date, const std::string& end_date, int level){
        long put_duration = timeStringToEpochSeconds(end_date) - timeStringToEpochSeconds(start_date);
        long level_duration = cache_levels_[level-1]["duration"];
        long level_points = static_cast<long>(cache_levels_[level-1]["num_of_points"]);

        /*LOGD("chache level %d wants %ld points for every %ld sec\n",level,level_points,level_duration);
        LOGD("put_duration %ld / level_duration %ld = %f durations",put_duration,level_duration,(static_cast<double>(put_duration)/static_cast<double>(level_duration)));
        LOGD(" = %f points = %d rounded points\n", level_points * (static_cast<double>(put_duration)/static_cast<double>(level_duration)), static_cast<int>(level_points * (static_cast<double>(put_duration)/static_cast<double>(level_duration))));
        */

        return static_cast<long>(level_points * (static_cast<double>(put_duration)/static_cast<double>(level_duration)));
    }

    // Note: This function is not currently being used, but has been tested to work and would be useful
    // if someone wanted to implement an update-cache function that makes the cache clear and re-pull
    // a certain period of time
    bool SQLiteDataCache::clearDatabaseRange(const std::string& table_name, const std::string& start_date, const std::string& end_date){
        std::string query = "DELETE FROM " + table_name + " WHERE " + date_key_column_ + " BETWEEN \"" + start_date + "\" AND \"" + end_date + "\";";
        try {
            LOGD("Deleting data points from cache table %s from %s to %s\n", table_name.c_str(), start_date.c_str(), end_date.c_str());
            executeQuery(query);
            return true;
        } catch (std::exception& ex) {
            LOGE("Exceptions caught deleting data: %s\n", ex.what());
            return false;
        }
    }


    bool SQLiteDataCache::openDatabase(){
        LOGD("Open database %s\n", database_path_.c_str());

        if (database_){
            sqlite3_close(database_);
        }

        int rc = sqlite3_open_v2(database_path_.c_str(), &database_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);

        if (rc != SQLITE_OK) {
            std::string err_msg(sqlite3_errmsg(database_));
            LOGE("Cannot open database: %s\n",err_msg.c_str());
            return false;
        }

        LOGD("Database opened succesfully\n");
        return true;
    }


    void SQLiteDataCache::createDatabase(){
        LOGD("Creating cache database\n");

        if(data_schema_.empty()){
            throw std::runtime_error(std::string("createDatabase called with empty data schema."));
        }
        std::string query;

        if(cache_raw_data_){
            query += "DROP TABLE IF EXISTS " + table_name_ + "_raw" + "; ";
            query += "CREATE TABLE " + table_name_ + "_raw" + "(";
            std::map<std::string,std::string>::iterator it;
            for(it = data_schema_.begin(); it != data_schema_.end(); it++){
                if(it != data_schema_.begin()){
                    query += ", ";
                }
                query += it->first + " " + it->second;
                if(it->first == date_key_column_){
                    query += " PRIMARY KEY";
                }
            }
            query += "); ";
        }

        for(int level=1; level <= cache_levels_.size(); ++level){
            std::stringstream buff;
            buff << "_" << level;
            query += "DROP TABLE IF EXISTS " + table_name_ + buff.str() + "; ";
            query += "CREATE TABLE " + table_name_ + buff.str() + "(";
            std::map<std::string,std::string>::iterator it;
            for(it = data_schema_.begin(); it != data_schema_.end(); it++){
                if(it != data_schema_.begin()){
                    query += ", ";
                }
                query += it->first + " " + it->second;
                if(it->first == date_key_column_){
                    query += " PRIMARY KEY";
                }
            }
            query += "); ";
        }


        try {
            //LOGD("Creating database via: \n%s\n",query.c_str());
            executeQuery(query);
            LOGD("Created database\n");
        } catch (std::exception& ex) {
            LOGE("Exceptions caught: %s\n", ex.what());
        }
    }

    void SQLiteDataCache::executeQuery(const std::string& sql_query){
        if (sql_query.empty())
            throw std::runtime_error(std::string("Invalid query string"));

        if (!database_)
            throw std::runtime_error(std::string("Invalid database object"));

        char* err = 0;

        int rc = sqlite3_exec(database_, sql_query.c_str(), NULL, NULL, &err);
        if (rc != SQLITE_OK) {
            std::string err_msg(err);
            sqlite3_free(err);
            LOGE("SQL query: %s\n", sql_query.c_str());
            throw std::runtime_error(err_msg);
        }
    }
}}
