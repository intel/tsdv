/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef GRAPHFILTER_SQLITEDATABASEACCESS_H
#define GRAPHFILTER_SQLITEDATABASEACCESS_H



#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include "databaseaccess.h"

namespace intel { namespace poc {

    class SQLiteDatabaseAccess : public DatabaseAccess {
        public:

            static DatabaseAccess& instance();

            ~SQLiteDatabaseAccess();

            bool init(const std::string& database_path, const Json::Value& data_schema, bool clean);

            bool putData(const Json::Value& data_values);

            Json::Value getData(const Json::Value& params);

        protected:
            /// constructor
            SQLiteDatabaseAccess():database_(NULL) {}

            sqlite3 *database_;
            std::string table_name_;
            std::map<std::string,std::string> data_schema_;
            std::string date_key_column_;


            bool initialized_;

            /// private API

            bool checkDatabase();

            bool openDatabase(const std::string& database_path);

            void createDatabase();

            void executeQuery(const std::string& sql_query);

    };
}}


#endif //GRAPHFILTER_SQLITEDATABASEACCESS_H
