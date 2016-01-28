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
#define  LOG_TAG    "SQLiteDatabaseAccess"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI(...)  printf(__VA_ARGS__)
#define  LOGD(...)  printf(__VA_ARGS__)
#define  LOGE(...)  printf(__VA_ARGS__)
#endif


#include <graphfilter/sqlitedatabaseaccess.h>
#include <sstream>
#include <algorithm>
#include <stdexcept>


namespace intel { namespace poc {

    DatabaseAccess& SQLiteDatabaseAccess::instance()
    {
        static DatabaseAccess *instance = new SQLiteDatabaseAccess();

        return *instance;
    }

    SQLiteDatabaseAccess::~SQLiteDatabaseAccess()
    {
        if (database_) {
            sqlite3_close(database_);
        }
    }

    bool SQLiteDatabaseAccess::init(const std::string& database_path, const Json::Value& data_schema, bool clean){
        initialized_ = false;

        // Parse data_schema
        if(!data_schema.isObject()){
            LOGE("data_schema not JSON object type: %s", data_schema.toStyledString().c_str());
            return false;
        }
        if(!data_schema.isMember("table") || !data_schema.isMember("date_key_column") || !data_schema.isMember("columns")){
            LOGE("Malformed data schema value: %s\n", data_schema.toStyledString().c_str());
            return false;
        } else {
            table_name_ = data_schema["table"].asString();
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

        if(!openDatabase(database_path)){
            LOGE("Failed to open database: %s\n", database_path.c_str());
            return false;
        }

        if(clean){
            try{
                createDatabase();
            } catch (std::exception& ex) {
                LOGE("Unable to create database: %s\n", ex.what());
                return false;
            }
        } else if(!checkDatabase()){
            LOGE("Database does not match schema provided");
            return false;
        }

        initialized_ = true;

        return initialized_;
    }

    bool SQLiteDatabaseAccess::putData(const Json::Value& data_values){
        // Parse the query params
        if(!data_values.isObject()){
            LOGE("data_values not object json type: %s", data_values.toStyledString().c_str());
            return false;
        }

        if (!data_values.isMember("startDate") || !data_values.isMember("endDate") || !data_values.isMember("points")){
            LOGE("Invalid data: startDate, endDate, or points missing.\n");
            return false;
        }

        if(!data_values["points"].isArray() || data_values["points"].size() == 0){
            LOGD("No points to put into databasae.\n");
            return true;
        }

        std::string startDate = data_values.get("startDate", "").asString();
        std::string endDate = data_values.get("endDate", "").asString();

        // Build the SQL insert string
        std::string query = "BEGIN TRANSACTION; "
            "INSERT OR IGNORE INTO " + table_name_ + " (";

        for(std::map<std::string,std::string>::iterator it = data_schema_.begin(); it != data_schema_.end(); ++it){
            if(it != data_schema_.begin()){
                query += ", ";
            }
            query += it->first;
        }

        query += ") VALUES ";
        for (int i = 0; i < data_values["points"].size(); ++i ){
            Json::Value data_point = data_values["points"][i];

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
                query += "); INSERT OR IGNORE INTO " + table_name_ + " (";
                for(std::map<std::string,std::string>::iterator it = data_schema_.begin(); it != data_schema_.end(); ++it){
                    if(it != data_schema_.begin()){
                        query += ", ";
                    }
                    query += it->first;
                }
                query += ") VALUES ";
            } else if(i != data_values["points"].size() - 1){
                query += "),";
            }  else {
                query += ");";
            }
        }
        query += "COMMIT TRANSACTION;";
        try {
            LOGD("Adding data to database from %s to %s\n", startDate.c_str(), endDate.c_str());
            executeQuery(query);
            return true;
        } catch (std::exception& ex) {
            LOGE("Exceptions caught: %s\n", ex.what());
            return false;
        }
    }

    Json::Value SQLiteDatabaseAccess::getData(const Json::Value& params){
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
        const Json::Value metrics = params["metrics"];

        std::vector<std::string> json_fields;

        if(metrics.empty() || (metrics.size() == 1 && metrics[0].asString() == "*")){
            // Include all metrics
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
        query << " FROM " + table_name_;
        query << " WHERE " << date_key_column_ << " BETWEEN \"" << query_start_time << "\" AND \"" << query_end_time << "\" ";
        query << " ORDER BY " << date_key_column_ << " ASC;";

        int num_of_fields = static_cast<int>(json_fields.size());
        std::string sql_query = query.str();

        Json::Value response = empty_response;
        response["startDate"] = query_start_time;
        response["endDate"] = query_end_time;
        try{
            sqlite3_stmt *stmt;
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

    bool SQLiteDatabaseAccess::checkDatabase() {
        LOGD("Checking tables\n");
        if(data_schema_.empty()){
            LOGE("Cannot check database against empty data schema.\n");
            return false;
        }

        try {
            std::string sql_query = "PRAGMA table_info(" + table_name_ + ");";

            sqlite3_stmt *stmt;

            //LOGD("Executing SQL query %s: \n", sql_query.c_str());
            int rc = sqlite3_prepare_v2(database_, sql_query.c_str(), -1, &stmt, NULL);

            if (rc != SQLITE_OK) {
                std::string err_msg(sqlite3_errmsg(database_));
                LOGE("Error processing SQL query: %s\n", sql_query.c_str());
                return false;
            } else if(sqlite3_column_count(stmt) != 6){
                LOGE("Number of returned columns does not match number expected.");
                return false;
            }

            std::map<std::string,std::string> table_schema;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string column_name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
                std::string column_type = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
                table_schema[column_name] = column_type;
            }


            /*
            // Output data schema and table schema for visual comparison
            LOGD("Data Schema:\n");
            for(std::map<std::string,std::string>::const_iterator it = data_schema_.begin(); it != data_schema_.end(); ++it){
                LOGD("%s : %s\n", it->first.c_str(),it->second.c_str());
            }

            LOGD("Table Schema:\n");
            for(std::map<std::string,std::string>::const_iterator it = table_schema.begin(); it != table_schema.end(); ++it){
                LOGD("%s : %s\n", it->first.c_str(),it->second.c_str());
            }*/

            return table_schema == data_schema_;

        } catch (std::exception& ex) {
            LOGE("Exceptions caught: %s\n", ex.what());
            return false;
        }
        return true;
    }


    bool SQLiteDatabaseAccess::openDatabase(const std::string& database_path){
        LOGD("Opening database %s\n", database_path.c_str());

        if (database_){
            sqlite3_close(database_);
        }

        int rc = sqlite3_open_v2(database_path.c_str(), &database_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);

        if (rc != SQLITE_OK) {
            std::string err_msg(sqlite3_errmsg(database_));
            LOGE("Cannot open database: %s\n",err_msg.c_str());
            return false;
        }

        LOGD("Database opened succesfully\n");
        return true;
    }


    void SQLiteDatabaseAccess::createDatabase(){
        LOGD("Creating database\n");

        if(data_schema_.empty()){
            throw std::runtime_error(std::string("createDatabase called with empty data schema."));
        }

        // recreating tables from scratch
        std::map<std::string,std::string>::iterator it;
        std::string query = "DROP TABLE IF EXISTS " + table_name_ + "; ";
        query += "CREATE TABLE " + table_name_ + "(";
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

        try {
            executeQuery(query);
            LOGD("Created database\n");
        } catch (std::exception& ex) {
            LOGE("Exceptions caught: %s\n", ex.what());
        }
    }

    void SQLiteDatabaseAccess::executeQuery(const std::string& sql_query){
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
