/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef INTEL_POC_GRAPHFILTER_H
#define INTEL_POC_GRAPHFILTER_H

#include <string>

namespace intel {
  namespace poc {

    /**
     * @class GraphFilter
     * @brief GraphFilter prefetch libary
     *
     * GraphFilter is an abstract class that represents the pre-fetch data library
     * that caches time-series data fetches from an application data backend.
     * It's a singleton that can be accessed by using the static instance() method.
     */
    class GraphFilter {
     public:
      /**
       * An instance method
       * This method retrieves the singleton object
       *
       * @return GraphFilter object
       */
      static GraphFilter& instance();

      /**
       * A destructor
       */
      virtual ~GraphFilter() {}

      /**
       * Back end database identifier
       *
       * @return Unique id identifying the backend database.
       */
      virtual const std::string id() const = 0;

      /**
       * Initialize GraphFilter library, must be called exactly once before using the library.
       *
       * @param[in] cache_setup JSON string that describes how the cache should be set up,
       *                  including how the cache should pre-calculate and store downsampled
       *                  versions of the data. If this parameter is empty, it will be equivalent
       *                  to passing false for "useCache" and the cache will not
       *                  be used.  For example:
       * {  "useCache": true,
       *    "downsamplingFilter": "TIME_WEIGHTED_POINTS",
       *    "cacheRawData": true,
       *    "fetchAhead": 1,
       *    "fetchBehind": 2,
       *    "downsamplingLevels": [
       *        {
       *            "duration": 31536000,
       *            "numOfPoints": 100
       *        },
       *        {
       *            "duration": 86400,
       *            "numOfPoints": 100
       *        },
       *    ]
       * }
       * Note: In the above example, the cache will pre-calculate and store two levels of
       *                  downsampled data: in the first, the raw data will be downsampled to
       *                  100 points for every 31536000 seconds (1 year); in the second, the raw
       *                  data will be downsampled to 100 points for every day.
       * Note: If not present, "cacheRawData" will be treated as false and only downsampled data
       *                  will be cached.
       * Note: "fetchAhead" and "fetchBehind" indicate the number of multiples of the current cache
       *                  putData request's duration should be pre-fetched and cached. For example,
       *                  the above values would mean that if a cache putData call was made for a time
       *                  period spanning one day, the two previous days and one following day (for
       *                  a total of 4 days, including the original request) would be fetched,
       *                  downsampled, and stored in the cache.
       * Note: If not present, "downsamplingFilter" will default to DataFilter::TIME_WEIGHTED_POINTS.
       *
       * @param[in] data_schema JSON string that represents the mapping of column names to their
       *                  data types. Used for creating the databases or data structures used
       *                  to implement the data cache. For example:
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
       * @param[in] database_path Database backend path, e.g. @c "/path/to/database.db"
       * @param[in] clean Indicate if backend should initialize with clean database, if
       *                  it is set to true, all existing data will be deleted.
       *
       * @retval true Succesfully initialized
       * @retval false Failed to initialize
       */
      virtual bool init(const std::string& cache_setup, const std::string& data_schema, const std::string& database_path, bool clean) = 0;

      /**
       * Adds new data points to the database
       *
       * @param[in] data_values A JSON formatted string that represents data points
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
      virtual bool addData(const std::string& data_values) = 0;

      /**
       * Retrieve data points requested from the specified time time range and granularity
       *
       * @param[in] params A JSON formatted string that specifies the searh query parameters,
       *                   currently supported parameters are:
       *                   startDate: the start date of the data points
       *                   endDate: the end date of the data points
       *                   numOfPoints: the maximum number of of points that is an avereage
       *                                of the downsampled data points.
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
       *       "steps" ],
       *   "numOfPoints" : 100 }
       * }
       *
       * @return A JSON formatted string that represent the array of data point elements
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
      virtual std::string getData(const std::string& params) = 0;

     protected:
      /// constructor
      GraphFilter() {}

      bool initialized_;
    };
  }
}

#endif // INTEL_POC_GRAPHFILTER_H
