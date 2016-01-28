/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifdef ANDROID
#define  LOG_TAG    "DataFilter"
#include <android/log.h>
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI(...)  printf(__VA_ARGS__)
#define  LOGD(...)  printf(__VA_ARGS__)
#define  LOGE(...)  printf(__VA_ARGS__)
#endif



#include <graphfilter/datafilter.h>
#include <vector>
#include <stdexcept>
#include <cmath>


namespace intel { namespace poc {

    std::string DataFilter::date_key_ = "";
    bool DataFilter::initialized_ = false;

    DataFilter::FilterType DataFilter::getType(std::string filter_string){
        if(filter_string == "POINTS"){
            return FilterType::POINTS;
        } else if(filter_string == "TIME_WEIGHTED_POINTS"){
            return FilterType::TIME_WEIGHTED_POINTS;
        } else if(filter_string == "TIME_WEIGHTED_TIME"){
            return FilterType::TIME_WEIGHTED_TIME;
        } else {
            throw std::runtime_error(std::string("Invalid data downsampling filter: ") + filter_string);
        }
    }

    bool DataFilter::init(const std::string& date_key){
        initialized_ = false;
        if(date_key.empty()){
            LOGE("Invalid date_key: %s.\n", date_key.c_str());
            return false;
        } else {
            date_key_ = date_key;
            initialized_ = true;
        }
        return initialized_;
    }

    Json::Value DataFilter::applyFilter(const Json::Value& data,
                                    const std::map<std::string, std::string>& data_schema,
                                    int num_of_points,
                                    FilterType filter){

        if(!initialized_){
            LOGE("DataFilter not initialized.\n");
            throw std::runtime_error("DataFilter not initialized.");
        }


        /*LOGD("Schema:\n");
        for(std::map<std::string,std::string>::const_iterator it = data_schema.begin(); it != data_schema.end(); ++it){
            LOGD("%s : %s\n", it->first.c_str(),it->second.c_str());
        }*/

        if(!data.isMember("points") || !data["points"].isArray()){
            throw std::runtime_error("Json data missing or malformed \"points\" element.");
        }

        int point_size = data["points"].size();
        LOGD("Requesting %d points downsampled to %d points\n", point_size, num_of_points);

        if (point_size == 0 || point_size <= num_of_points) {
            LOGD("No downsampling required.\n");
            return data;
        }


        // construct new json response
        Json::Value downsampled_results;


        downsampled_results["startDate"] = data.get("startDate", "").asString();
        downsampled_results["endDate"] = data.get("endDate", "").asString();
        downsampled_results["points"] = Json::Value(Json::arrayValue);

        switch(filter){
            case FilterType::POINTS:
                LOGD("Using points-based downsampling filter\n");
                applyFilterPoints(data, downsampled_results["points"], 0, data["points"].size(), data_schema, num_of_points);
                break;
            case FilterType::TIME_WEIGHTED_POINTS:
                LOGD("Using time-weighted-points-based downsampling filter\n");
                applyFilterTimeWeighted(data, downsampled_results["points"],0, data["points"].size(), data_schema, num_of_points, FilterType::TIME_WEIGHTED_POINTS);
                break;
            case FilterType::TIME_WEIGHTED_TIME:
                LOGD("Using time-weighted-time-based downsampling filter\n");
                applyFilterTimeWeighted(data, downsampled_results["points"],0, data["points"].size(), data_schema, num_of_points, FilterType::TIME_WEIGHTED_TIME);
                break;
            default:
                LOGD("Invalid/uninitialized FilterType value passed: %d", filter);
                throw std::runtime_error("Invalid/uninitialized FilterType value passed: " + static_cast<int>(filter));
                break;
        }

        LOGD("Final downsampled points count: %d\n", downsampled_results["points"].size());
        return downsampled_results;
    }

    void DataFilter::applyFilterPoints(const Json::Value& data,
                                    Json::Value& out_points,
                                    int start_i,
                                    int end_i,
                                    const std::map<std::string, std::string>& data_schema,
                                    int num_of_points){


        if(num_of_points == 0){
            return;
        }
        const Json::Value& points = data["points"];

        //LOGD("Averaging points %d through %d\n", start_i, end_i);
        //LOGD("out_points already has %d points\n", out_points.size());
        int num_fields = points[start_i].size();
        double avg_data_per_point = static_cast<double>(end_i - start_i) / static_cast<double>(num_of_points);
        //LOGD("avg_data_per_point = %f\n",avg_data_per_point);
        double averages[num_fields];

        int prev_point_index = start_i - 1;

        for (int i = 0; i < num_fields; i++) {
            averages[i] = 0;
        }

        // loop through every element in the points array
        for (int point_index = start_i; point_index < end_i; point_index++) {
            Json::Value element = points[point_index];
            //LOGD("Point %d: %s\n",point_index, element.toStyledString().c_str());
            int mapping_index = 0;
            std::vector<std::string> data_names = element.getMemberNames();
            for(std::vector<std::string>::const_iterator i = data_names.begin(); i != data_names.end(); ++i) {
                std::string element_name = *i;
                //LOGD("element_name: %s\n", element_name.c_str());
                std::string element_type = data_schema.find(*i)->second;
                //LOGD("element_type: %s\n", element_type.c_str());

                // only average numeric number, not strings values like date time
                if (element_type == "INT" || element_type == "REAL") {
                    try {
                        //LOGD("%s average = %f + %f = ",element_name.c_str(), averages[mapping_index], element[element_name].asDouble());
                        averages[mapping_index] += element[element_name].asDouble();
                        //LOGD("%f\n",averages[mapping_index]);
                    } catch (...) {
                        averages[mapping_index] += 0;
                    }
                }
                mapping_index++;
            }

            //if (downsampled_points.size() == num_of_points - 1 && point_index != end_i - 1) {
            //    continue;
            //}

            if ((point_index > 0 && fmod((point_index + 1 - start_i), ceil(avg_data_per_point)) == 0) || (point_index == end_i - 1)) {
                Json::Value new_element;
                int range = point_index - prev_point_index;


                // loop through all value pair and calculate averages
                mapping_index = 0;
                std::vector<std::string> data_names = element.getMemberNames();
                for(std::vector<std::string>::const_iterator i = data_names.begin(); i != data_names.end(); ++i) {
                    std::string element_name = *i;
                    std::string element_type = data_schema.find(*i)->second;
                    //LOGD("element_name: %s, element_type: %s, value = %f / %d", element_name.c_str(), element_type.c_str(), averages[mapping_index], range);
                    averages[mapping_index] /= range;
                    //LOGD(" = %f\n", averages[mapping_index]);

                    if (element_type == "INT") {
                        new_element[element_name] = static_cast<int>(averages[mapping_index]);
                    } else if (element_type == "REAL") {
                        new_element[element_name] = averages[mapping_index];
                    } else {
                        new_element[element_name] = element.get(element_name, "").asString();
                    }

                    averages[mapping_index] = 0;
                    mapping_index++;
                }
                prev_point_index = point_index;
                out_points.append(new_element);
            }
        }

    }

    long timeStringToEpochSeconds(const std::string& time_string){
        struct tm tm = {};
        // 2015-03-03 00:00Z
        // TODO: Expand this to check for and allow other common date formats
        if (strptime(time_string.c_str(), "%Y-%m-%d %H:%MZ", &tm) == NULL){
            LOGE("Error converting time_string to time struct: %s\n", time_string.c_str());
            return -1;
        }
        return static_cast<long>(mktime(&tm));
    }


    void DataFilter::applyFilterTimeWeighted(const Json::Value& data,
                                        Json::Value& out_points,
                                        int start_i,
                                        int end_i,
                                        const std::map<std::string, std::string>& data_schema,
                                        int num_of_points,
                                        DataFilter::FilterType filter_type){

        const Json::Value& points = data["points"];

        if(num_of_points == 0){
            return;
        } else if(num_of_points <= AVG_POINTS_PER_BUCKET_){
            applyFilterPoints(data, out_points, start_i, end_i, data_schema, num_of_points);
        } else {

            long start_time = timeStringToEpochSeconds(points[start_i].get(date_key_,"").asString());
            long end_time = timeStringToEpochSeconds(points[end_i - 1].get(date_key_,"").asString());
            long bucket_duration = static_cast<double>(end_time - start_time) / (static_cast<double>(num_of_points) / AVG_POINTS_PER_BUCKET_);

            long bucket_start = start_time;
            long bucket_end = start_time + bucket_duration;
            int bucket_size = 0;
            for(int i = start_i; i < end_i; ++i){
                //LOGD("bucket_start = %ld, bucket_end = %ld\n",bucket_start, bucket_end);
                //LOGD("point.date = %ld\n",timeStringToEpochSeconds(points[i].get(date_key_,"").asString()));
                if(timeStringToEpochSeconds(points[i].get(date_key_,"").asString()) >= bucket_start &&
                    timeStringToEpochSeconds(points[i].get(date_key_,"").asString()) <= bucket_end){
                    bucket_size++;
                } else {
                    int scaled_num_of_points = static_cast<int>(static_cast<double>(bucket_size) / static_cast<double>(end_i - start_i) * num_of_points);
                    //LOGD("scaled points = %d / %d * %d = %d\n", bucket_size, (end_i - start_i), num_of_points, scaled_num_of_points);
                    //LOGD("Downsample %d points to %d scaled points.\n", bucket_size, scaled_num_of_points);
                    if(scaled_num_of_points > 0){
                        switch(filter_type){
                            case FilterType::TIME_WEIGHTED_POINTS:
                                applyFilterPoints(data, out_points, i - bucket_size, i, data_schema, scaled_num_of_points);
                                break;
                            case FilterType::TIME_WEIGHTED_TIME:
                                applyFilterTimeWeighted(data, out_points, i - bucket_size, i, data_schema, scaled_num_of_points, FilterType::TIME_WEIGHTED_TIME);
                                break;
                        }
                    }
                    //LOGD("Update current bucket\n");
                    bucket_size = 1;
                    bucket_start += bucket_duration;
                    bucket_end += bucket_duration;
                }
            }
            //LOGD("Final bucket_size = %d\n", bucket_size);
            int scaled_num_of_points = static_cast<int>(static_cast<double>(bucket_size) / static_cast<double>(end_i - start_i) * num_of_points);
            //LOGD("Downsample %d points to %d scaled points.\n", bucket_size, scaled_num_of_points);
            switch(filter_type){
                case FilterType::TIME_WEIGHTED_POINTS:
                    applyFilterPoints(data, out_points, end_i - bucket_size, end_i, data_schema, scaled_num_of_points);
                    break;
                case FilterType::TIME_WEIGHTED_TIME:
                    applyFilterTimeWeighted(data, out_points, end_i - bucket_size, end_i, data_schema, scaled_num_of_points, FilterType::TIME_WEIGHTED_TIME);
                    break;
            }
        }
     }

}}