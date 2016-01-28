/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef GRAPHFILTER_DATABASEACCESS_H
#define GRAPHFILTER_DATABASEACCESS_H

#include "json.h"

namespace intel {
  namespace poc {

    /**
     * @class DatabaseAccess
     * @brief Database abstract base class
     *
     * DatabaseAccess is an abstract class that represents a TSDV database
     * that contains the raw data.
     */
    class DatabaseAccess {
     public:
      /**
       * A destructor
       */
      virtual ~DatabaseAccess() {}


      /**
       * Initialize DatabaseAccess. Must be called exactly once before using the database class.
       *
       * @param[in] dataSchema Json::Value that represents the table name and the mapping of
       *                  column names to their data types. Used for creating or reading the
       *                  databases. For example:
       * {  "table": "data",
       *    "date_key_column": "date",
       *    "columns": {
       *        "date": "TEXT",
       *        "calories": "INT",
       *        "steps": "INT",
       *        "body_temp": "REAL"
       *    }
       * }
       * Note: The column identified by the "date_key_column" field will be
       *                  treated as the primary key. It MUST be present in
       *                  the column list and MUST be of type "TEXT".  Dates in database
       *                  should be of the format: "YYYY-MM-DD HH:MMZ".
       *
       * @param[in] clean Indicate if backend should initialize with clean database, if
       *                  it is set to true, all existing data will be deleted.
       *
       * @retval true Succesfully initialized
       * @retval false Failed to initialize
       */
      virtual bool init(const std::string& database_path, const Json::Value& data_schema, bool clean) = 0;


      /**
       * Adds new data points to the database
       *
       * @param[in] data_values A Json::Value object that represents data points
       *                        in this form:
       * { "startDate" : "2015-03-03 00:00Z",
       *   "endDate" : "2015-03-03 23:59Z",
       *   "points" : [
       *    {   "date":"2015-03-03 00:00Z",
       *        "calories":2.0,
       *        "gsr":"4.27263e-05",
       *        "heart_rate":0
       *        "body_temp":82.4
       *        "steps", 0
       *    },
       *    ...
       *    ...
       *    ...
       *    {   "date":"2015-03-03 23:59Z",
       *        "calories":1.0,
       *        "gsr":"4.22559e-05",
       *        "heart_rate":68
       *        "body_temp":85.1
       *        "steps", 0
       *    }
       *   ]
       * }
       *
       * Please note that fields must match the columns defined in the
       *                data_schema parameter passed to init.
       *
       * @retval true Succesfully added data to library
       * @retval false Failed to add data to library
       */
      virtual bool putData(const Json::Value& data_values) = 0;

      /**
       * Retrieve data points requested from the specified time time range and granularity
       *
       * @param[in] params A Json::Value object that specifies the search query parameters,
       *                   currently supported parameters are:
       *                   startDate: the start date of the data points
       *                   endDate: the end date of the data points
       *                   numOfPoints: the maximum number of of points that is a downsampled
       *                                average of original data points.
       *                   metrics: (future, not yet supported)
       *
       * Queries constructed in this form:
       *
       * { "startDate" : "2015-03-03 00:00Z",
       *   "endDate" : "2015-03-03 23:59Z",
       *   "metrics" : [
       *       "calories",
       *       "gsr",
       *       "heart_rate",
       *       "body_temp",
       *       "steps" ]}
       * }
       *
       * @return A Json::Value object that represent the array of data point elements
       *        in this format:
       * { "startDate" : "2015-03-03 00:00Z",
       *   "endDate" : "2015-03-03 23:59Z",
       *   "points" : [
       *    { "date":"2015-03-03 00:00Z",
       *        "calories":2.0,
       *        "gsr":"4.27263e-05",
       *        "heart_rate":0
       *        "body_temp":82.4
       *        "steps", 0
       *    },
       *    ...
       *    ...
       *    ...
       *    {   "date":"2015-03-03 23:59Z",
       *        "calories":1.0,
       *        "gsr":"4.22559e-05",
       *        "heart_rate":68
       *        "body_temp":85.1
       *        "steps", 0
       *    }
       *   ]
       * }
       * Note: In the event of an error, the function will return an empty
       *        response in this format:
       * { "startDate" : "",
       *   "endDate" : "",
       *   "points" : []
       * }
       */
      virtual Json::Value getData(const Json::Value& params) = 0;

     protected:
      /// constructor
      DatabaseAccess() {}
    };
  }
}


#endif //GRAPHFILTER_DATABASEACCESS_H


