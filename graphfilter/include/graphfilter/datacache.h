/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef GRAPHFILTER_DATACACHE_H
#define GRAPHFILTER_DATACACHE_H


#include "json.h"

namespace intel {
  namespace poc {

    /**
     * @class DataCache
     * @brief Abstract data cache base class
     *
     * DataCache is an abstract class that represents a faster, probably in-memory
     * cache for already-accessed and pre-calculated data.
     */
    class DataCache {
     public:
      /**
       * A destructor
       */
      virtual ~DataCache() {}


      /**
       * Initialize DataCache. Must be called exactly once before using the library.
       *
       * @param[in] cache_setup Json::Value object that describes how the cache should be set up,
       *                  including how the cache should pre-calculate and store downsampled
       *                  versions of the data. Required parameter.  For example:
       * {  "cacheRawData": true,
       *    "downsamplingFilter": "TIME_WEIGHTED_POINTS",
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
       * Note: "fetchAhead" and "fetchBehind" indicate the number of multiples of the current
       *                  putData request's duration should be pre-fetched and cached. For example,
       *                  the above values would mean that if a putData request came in for a time
       *                  period spanning one day, the two previous days and one following day (for
       *                  a total of 4 days, including the original request) would be fetched,
       *                  downsampled, and stored in the cache.
       * Note: If not present, "downsamplingFilter" will default to DataFilter::TIME_WEIGHTED_POINTS.
       *
       * @param[in] data_schema Json::Value object that represents the mapping of column names to their
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
       * @param[in] clean Indicate if backend should initialize with clean database, if
       *                  it is set to true, all existing cached data will be deleted.
       *
       * @retval true Succesfully initialized
       * @retval false Failed to initialize
       */
      virtual bool init(const Json::Value& cache_setup, const Json::Value& data_schema, bool clean) = 0;


      /**
       * Adds new data points to the cache. Because processing is done done asynchronously on
       *                  a separate thread, there is no return value or indication of success of
       *                  the actual database put.
       *
       * @param[in] start_date A date-time string indicating the start of the time interval for
       *                        which data should be retreived and cached. Should be of the
       *                        format: "YYYY-MM-DD HH:MMZ".
       * @param[in] start_date A date-time string indicating the end of the time interval for
       *                        which data should be retreived and cached. Should be of the
       *                        format: "YYYY-MM-DD HH:MMZ".
       */
      virtual void cacheData(const std::string& start_date, const std::string& end_date) = 0;

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
       *       "steps" ],
       *   "numOfPoints" : 100 }
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
       * Note: In the event of an error, OR in the event the cache does not
       *        contain the data requested,  the function will return an empty
       *        response in this format:
       * { "startDate" : "",
       *   "endDate" : "",
       *   "points" : []
       * }
       */
      virtual Json::Value getData(const Json::Value& params) = 0;

     protected:
      /// constructor
      DataCache() {}

      bool initialized_;
    };
  }
}


#endif //GRAPHFILTER_DATACACHE_H
