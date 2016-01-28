/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef GRAPHFILTER_SQLITEDATACACHE_H
#define GRAPHFILTER_SQLITEDATACACHE_H


#include <graphfilter/datafilter.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "datacache.h"

namespace intel { namespace poc {

    class SQLiteDataCache : public DataCache {
        public:

            static DataCache& instance();

            ~SQLiteDataCache();

            bool init(const Json::Value& cache_setup, const Json::Value& data_schema, bool clean);

            void cacheData(const std::string& start_date, const std::string& end_date);

            Json::Value getData(const Json::Value& params);

        protected:
            /// constructor
            SQLiteDataCache():database_(NULL) {}

            sqlite3 *database_;
            static const std::string database_path_;
            std::string table_name_;
            std::map<std::string,std::string> data_schema_;
            bool cache_raw_data_;
            int fetch_ahead_;
            int fetch_behind_;
            DataFilter::FilterType downsampling_filter_;
            std::vector<std::map<std::string,long>> cache_levels_;

            std::string date_key_column_;
            std::map<std::string,std::string> cache_data_bounds_;
            std::mutex put_data_mutex_;
            std::mutex cache_data_bounds_mutex_;

            bool initialized_;

            /// private API
            void cacheDataAsync(const std::string& start_date, const std::string& end_date);
            bool getAndPutData(const std::string& start_date, const std::string& end_date);
            bool downsampleAndPutData(int level, const Json::Value& data_values);
            bool putDataTable(const std::string& table_name, const Json::Value& points);
            long timeStringToEpochSeconds(const std::string& time_string);
            std::string updateTimeString(const std::string& time_string, long offset);
            long getDurationNumPoints(const std::string& start_date, const std::string& end_date, int level);
            bool clearDatabaseRange(const std::string& table_name, const std::string& start_date, const std::string& end_date);


            std::string cacheContains(const std::string& startDate, const std::string& endDate, int num_of_points);
            std::map<std::string,std::string> getCacheDifference(std::string start_date, std::string end_date);


            bool openDatabase();

            void createDatabase();

            void executeQuery(const std::string& sql_query);

            std::vector<std::string> executeSelectQuery(const std::string& sql_query, int num_of_col);

    };
}}




#endif //GRAPHFILTER_SQLITEDATACACHE_H
