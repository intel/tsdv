/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef GRAPHFILTER_DATAFILTER_H
#define GRAPHFILTER_DATAFILTER_H
#include <string>
#include <map>
#include "json.h"

namespace intel { namespace poc {

    class DataFilter {
        public:
            /**
            * Enum representing the valid filter types
            */
            enum class FilterType { POINTS, TIME_WEIGHTED_POINTS, TIME_WEIGHTED_TIME };

            /**
            * Initialize DataFilter. Must be called exactly once before using the library.
            *
            * @param[in] date_key_column String identifying the field in your data points that
            *       represents the date timestamp field
            *
            * @retval true Succesfully initialized
            * @retval false Failed to initialize
            */
            static bool init(const std::string& date_key);

            /**
            * Downsample the given data using the given filter to the given number of points.
            *
            * @param[in] data A Json::Value object that represent the array of data point elements
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
            *
            * @param[in] data_schema Json::Value object that represents the mapping of column names to their
            *                  data types. Used to parse data elements correctly. For example:
            * {  "table": "data",
            *    "date_key_column": "date",
            *    "columns": {
            *        "date": "TEXT",
            *        "calories": "INT",
            *        "steps": "INT",
            *        "body_temp": "REAL"
            *    }
            * }
            *
            * @param[in] num_of_points The maximum number of points you wish the data downsampled
            *                  to.  Note that you are not guaranteed to receive this number of points.
            *
            * @return a Json::Value representing the downsampled data
            */
            static Json::Value applyFilter(const Json::Value& data,
                                     const std::map<std::string, std::string>& data_schema,
                                     int num_of_points,
                                     FilterType filter);
            /**
            * Helper function to get an enum type given its equivalent string.
            *
            * @param[in] filter_string String representing a filter type. Should be textually
            *                   equivalent to one of the valid filter types.
            *
            * @return The corresponding FilterType value.
            */
            static FilterType getType(std::string filter_string);

        private:
            /**
            * Performs a purely point-based average irrespective of time. That is, this algorithm
            * will compute an average based solely on the number of points, and does not pay any
            * attention to how those points are distributed along the time frame given in "data".
            *
            * This algorithm runs in O(n) time.
            */
            static void applyFilterPoints(const Json::Value& data,
                                     Json::Value& out_points,
                                     int start_i,
                                     int end_i,
                                     const std::map<std::string, std::string>& data_schema,
                                     int num_of_points);
            /**
            * Performs a time-based averaging.  The timeframe given in "data" is divided into
            * even-width buckets that would be wide enough to hold AVG_POINTS_PER_BUCKET_ number of
            * points if the points were all evenly distributed.  Each of these buckets is then
            * separately downsampled, with the number of points per bucket being a portion of the
            * overal number of points requested, linearly scaled proportionate to the number of points
            * found in this bucket compared to the overall number of points.  For example, if there
            * are 1000 points in the raw data, 500 happen to fall into this bucket, and you ask for
            * a total of 100 downsampled points, this bucket will be downsampled to 50 points.
            *
            * This time-based bucketing approach allows for significantly more representative,
            * more realistically distributed data points.
            *
            * If filter_type = FilterType::TIME_WEIGHTED_POINTS, each bucket is downsampled using
            * the points-based downsampling algorithm above, and thus within any given bucket, the
            * data points are downsampled without respect to time.  This version runs in an overall
            * O(2n) time.
            *
            * If filter_type = FilterType::TIME_WEIGHTED_TIME, each bucket is downsampled by
            * recursively using same time-weighted downsampling algorithm, until it reaches a
            * base-case of a bucket that should have AVG_POINTS_PER_BUCKET_ points, at which point
            * the time-based averaging is used.  As a result, even within a bucket, the points are
            * distributed more representatively of the original data.  This algorithm runs in
            * average case O(2n) time.  It runs in worst-case O(n*log(n)), where the log base is
            * num_of_points / AVG_POINTS_PER_BUCKET_.
            */
            static void applyFilterTimeWeighted(const Json::Value& data,
                                     Json::Value& out_points,
                                     int start_i,
                                     int end_i,
                                     const std::map<std::string, std::string>& data_schema,
                                     int num_of_points,
                                     FilterType filter_type);

            static std::string date_key_;
            static bool initialized_;
            static const int AVG_POINTS_PER_BUCKET_ = 10;
    };

}}

#endif //GRAPHFILTER_DATAFILTER_H
