/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include <stdio.h>
#include <fstream>
#include <graphfilter/graphfilter.h>
#include <graphfilter/datacache.h>
#include <graphfilter/sqlitedatacache.h>
#include <graphfilter/databaseaccess.h>
#include <graphfilter/sqlitedatabaseaccess.h>
#include "gtest/gtest.h"
#include "json.h"
#include "dummydata.h"  // Data from 2MonthData.csv file
#include <chrono>
#include <thread>
#include <stdexcept>

namespace {

std::string database_path = "/data/local/tmp/unit_test_tmp.db";

std::string cache_setup = "{\"useCache\": true,"
  "\"cacheRawData\": true,"
  "\"downsamplingLevels\": ["
  "   { \"duration\": 86400,"
  "     \"numOfPoints\": 100"
  "   },"
  "   { \"duration\": 86400,"
  "     \"numOfPoints\": 1000"
  "   }"
  " ]"
  "}";

std::string data_schema = "{\"table\": \"data\","
  "\"date_key_column\": \"date\","
  "\"columns\": {"
  "     \"date\": \"TEXT\","
  "     \"calories\": \"REAL\","
  "     \"gsr\": \"REAL\","
  "     \"heart_rate\": \"INT\","
  "     \"body_temp\": \"REAL\","
  "     \"steps\": \"INT\""
  " }"
  "}";

class GraphFilterTest : public ::testing::Test {
 protected:
  intel::poc::GraphFilter& gf;

  GraphFilterTest()
    : gf(intel::poc::GraphFilter::instance()) {
    // You can do set-up work for each test here.
  }

  virtual ~GraphFilterTest() {
    // You can do clean-up work that doesn't throw exceptions here.
    // wipe data after each test
    gf.init("",data_schema,"/data/local/tmp/test.db", true);
  }
};

class GraphFilterDummyDataTest : public ::testing::Test {
 protected:
  intel::poc::GraphFilter& gf;

  GraphFilterDummyDataTest()
    : gf(intel::poc::GraphFilter::instance()) {
    // You can do set-up work for each test here.
  }

  virtual ~GraphFilterDummyDataTest() {
    // You can do clean-up work that doesn't throw exceptions here.
  }
};


class DatabaseAccessTest : public ::testing::Test {
  protected:
    intel::poc::DatabaseAccess& da;
    Json::Reader reader_;
    Json::Value data_schema_json_;

    DatabaseAccessTest() : da(intel::poc::SQLiteDatabaseAccess::instance()) {
      // You can do set-up work for each test here.
      reader_.parse(data_schema, data_schema_json_);
    }

    virtual ~DatabaseAccessTest() {
      // You can do clean-up work that doesn't throw exceptions here.
    }
};


class DataCacheTest : public ::testing::Test {
  protected:
    intel::poc::DataCache& dc;
    intel::poc::DatabaseAccess& da;
    Json::Reader reader_;
    Json::Value cache_setup_json_;
    Json::Value data_schema_json_;
    const int sleep_time_ = 750;

    DataCacheTest() : dc(intel::poc::SQLiteDataCache::instance()),
                      da(intel::poc::SQLiteDatabaseAccess::instance()) {
      // You can do set-up work for each test here.
      reader_.parse(cache_setup, cache_setup_json_);
      reader_.parse(data_schema, data_schema_json_);
      da.init(database_path,data_schema_json_, true);
    }

    virtual ~DataCacheTest() {
      // You can do clean-up work that doesn't throw exceptions here.
    }
};

}

TEST_F(GraphFilterTest, Singleton) {
  intel::poc::GraphFilter& gf2 = intel::poc::GraphFilter::instance();
  ASSERT_TRUE(&gf == &gf2);
}

TEST_F(GraphFilterTest, Init) {
  EXPECT_TRUE(gf.init("",data_schema,"/data/local/tmp/test.db", true));
  EXPECT_TRUE(gf.init("",data_schema,"/data/local/tmp/test.db", false));
}

TEST_F(GraphFilterTest, InitDatabaseAndCleanData) {
  EXPECT_TRUE(gf.init("",data_schema,"/data/local/tmp/test.db", true));
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\","
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_EQ(0, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterTest, InitExistingDatabase) {
  EXPECT_TRUE(gf.init("",data_schema,"/data/local/tmp/test.db", false));
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\","
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_EQ(0, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterTest, InitExistingDatabaseAndCleanData) {
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  EXPECT_TRUE(gf.addData(param)) << " input param: " << param;
  EXPECT_TRUE(gf.init("",data_schema,"/data/local/tmp/test.db", true));
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\","
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_EQ(0, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterTest, InitExistingDatabaseAndKeepData) {
  EXPECT_TRUE(gf.init("",data_schema,"/data/local/tmp/test.db", true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  EXPECT_TRUE(gf.addData(param)) << " input param: " << param;
  EXPECT_TRUE(gf.init("",data_schema,"/data/local/tmp/test.db", false));
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\","
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_EQ(1, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterTest, InitWithNoAccessPermissionToDirectory) {
  // Android apps have no permission to access the root dir
  EXPECT_FALSE(gf.init("",data_schema,"/test.db", true));
  EXPECT_FALSE(gf.init("",data_schema,"/test.db", false));
}

TEST_F(GraphFilterTest, AddEmptyData) {
  EXPECT_FALSE(gf.addData(""));
}

TEST_F(GraphFilterTest, AddMalformedData) {
  EXPECT_FALSE(gf.addData("notjson"));
}

TEST_F(GraphFilterTest, AddSingleData) {
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  EXPECT_TRUE(gf.addData(param)) << " input param: " << param;
}

TEST_F(GraphFilterTest, AddDuplicateData) {
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  EXPECT_TRUE(gf.addData(param)) << " input param: " << param;
  EXPECT_TRUE(gf.addData(param)) << " input param: " << param;
}

TEST_F(GraphFilterTest, GetDataAvailable) {
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  EXPECT_EQ(true, gf.addData(param)) << " input param: " << param;
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\","
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(1000, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterTest, GetDataUnavailable) {
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  EXPECT_TRUE(gf.addData(param)) << " input param: " << param;
  EXPECT_TRUE(gf.init("",data_schema,"/data/local/tmp/test.db", false));
  std::string result = gf.getData("{\"startDate\":\"2014-03-03 00:00Z\","
    "\"endDate\":\"2014-03-03 23:59Z\","
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_EQ(0, json_root["points"].size()) << " result size: " << json_root["points"].size();
}


TEST_F(GraphFilterTest, UseCache) {
  ASSERT_TRUE(gf.init(cache_setup,data_schema,"/data/local/tmp/test.db", true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  EXPECT_EQ(true, gf.addData(param)) << " input param: " << param;
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\","
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_EQ(1, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

// All tests below are preloaded with data from 2MonthsData.csv
// Using GraphFilterDummyDataTest typed test
TEST_F(GraphFilterDummyDataTest, BatchTransaction) {
  std::istringstream inputStream(std::string(reinterpret_cast<const char*>(POC_2MonthData_csv)));
  ASSERT_TRUE(inputStream);
  std::string line;

  gf.init("",data_schema,"/data/local/tmp/test.db", true);
  std::string param ="{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [";

  while (std::getline(inputStream, line)) {
    std::string dateTime;
    std::string calories;
    std::string gsr;
    std::string heart_rate;
    std::string body_temp;
    std::string steps;

    //ignore the first line
    if (line.substr(0, 5) == std::string("date,"))
      continue;

    std::stringstream lineStream(line);
    std::getline(lineStream, dateTime, ',');
    std::getline(lineStream, calories, ',');
    std::getline(lineStream, gsr, ',');
    std::getline(lineStream, heart_rate, ',');
    std::getline(lineStream, body_temp, ',');
    std::getline(lineStream, calories, ',');

    // set to fallback values since it can be empty
    if (calories.empty())
      calories = std::string("0");
    if (heart_rate.empty())
      heart_rate = std::string("0");
    if (body_temp.empty())
      body_temp = std::string("0");
    if (steps.empty())
      steps = std::string("0");

    param += "{\"date\":\"" + dateTime + "\",";
    param += "\"calories\":" + calories + ",\"gsr\":\"" + gsr + "\",";
    param += "\"heart_rate\":" + heart_rate + ",\"body_temp\":" + body_temp;
    param += ",\"steps\":" + steps + "},";


  }
  param.pop_back(); // Remove trailing comma

  param += "]}";

  ASSERT_TRUE(gf.addData(param));// << " input data: " << param;
}

TEST_F(GraphFilterDummyDataTest, GetOutOfRangeData) {
  std::string result = gf.getData("{\"startDate\":\"2014-03-03 00:00Z\","
    "\"endDate\":\"2014-03-03 23:59Z\","
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_EQ(0, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterDummyDataTest, GetLargeData) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\","
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(1000, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterDummyDataTest, GetReducedData) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\","
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterDummyDataTest, GetDataWithEmptyMetrics) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
  json_element = json_root["points"][0];
  EXPECT_TRUE(json_element.isMember("heart_rate"));
  EXPECT_TRUE(json_element.isMember("body_temp"));
  EXPECT_TRUE(json_element.isMember("calories"));
  EXPECT_TRUE(json_element.isMember("steps"));
  EXPECT_TRUE(json_element.isMember("gsr"));
}

TEST_F(GraphFilterDummyDataTest, GetDataWithHeartRateOnly) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"heart_rate\"],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
  json_element = json_root["points"][0];
  EXPECT_TRUE(json_element.isMember("heart_rate"));
  EXPECT_TRUE(!json_element.isMember("body_temp"));
  EXPECT_TRUE(!json_element.isMember("calories"));
  EXPECT_TRUE(!json_element.isMember("steps"));
  EXPECT_TRUE(!json_element.isMember("gsr"));
}

TEST_F(GraphFilterDummyDataTest, GetDataWithBodyTempOnly) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"body_temp\"],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
  json_element = json_root["points"][0];
  EXPECT_TRUE(!json_element.isMember("heart_rate"));
  EXPECT_TRUE(json_element.isMember("body_temp"));
  EXPECT_TRUE(!json_element.isMember("calories"));
  EXPECT_TRUE(!json_element.isMember("steps"));
  EXPECT_TRUE(!json_element.isMember("gsr"));
}

TEST_F(GraphFilterDummyDataTest, GetDataWithCaloriesOnly) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"calories\"],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
  json_element = json_root["points"][0];
  EXPECT_TRUE(!json_element.isMember("heart_rate"));
  EXPECT_TRUE(!json_element.isMember("body_temp"));
  EXPECT_TRUE(json_element.isMember("calories"));
  EXPECT_TRUE(!json_element.isMember("steps"));
  EXPECT_TRUE(!json_element.isMember("gsr"));
}

TEST_F(GraphFilterDummyDataTest, GetDataWithStepsOnly) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"steps\"],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
  json_element = json_root["points"][0];
  EXPECT_TRUE(!json_element.isMember("heart_rate"));
  EXPECT_TRUE(!json_element.isMember("body_temp"));
  EXPECT_TRUE(!json_element.isMember("calories"));
  EXPECT_TRUE(json_element.isMember("steps"));
  EXPECT_TRUE(!json_element.isMember("gsr"));
}

TEST_F(GraphFilterDummyDataTest, GetDataWithGSROnly) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"gsr\"],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
  json_element = json_root["points"][0];
  EXPECT_TRUE(!json_element.isMember("heart_rate"));
  EXPECT_TRUE(!json_element.isMember("body_temp"));
  EXPECT_TRUE(!json_element.isMember("calories"));
  EXPECT_TRUE(!json_element.isMember("steps"));
  EXPECT_TRUE(json_element.isMember("gsr"));
}

TEST_F(GraphFilterDummyDataTest, GetDataWithMixedMetrics) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"heart_rate\",\"calories\",\"gsr\"],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
  json_element = json_root["points"][0];
  EXPECT_TRUE(json_element.isMember("heart_rate"));
  EXPECT_TRUE(!json_element.isMember("body_temp"));
  EXPECT_TRUE(json_element.isMember("calories"));
  EXPECT_TRUE(!json_element.isMember("steps"));
  EXPECT_TRUE(json_element.isMember("gsr"));
}

TEST_F(GraphFilterDummyDataTest, GetDataWithAllMetrics) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"*\"],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
  json_element = json_root["points"][0];
  EXPECT_TRUE(json_element.isMember("heart_rate"));
  EXPECT_TRUE(json_element.isMember("body_temp"));
  EXPECT_TRUE(json_element.isMember("calories"));
  EXPECT_TRUE(json_element.isMember("steps"));
  EXPECT_TRUE(json_element.isMember("gsr"));
}

TEST_F(GraphFilterDummyDataTest, GetDataWithInvalidMetric) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"invalid\"],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_EQ(0, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterDummyDataTest, GetDownSampledData1) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"*\"],"
    "\"numOfPoints\":1000}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(1000, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterDummyDataTest, GetDownSampledData2) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"*\"],"
    "\"numOfPoints\":100}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(100, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterDummyDataTest, GetDownSampledData3) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"*\"],"
    "\"numOfPoints\":1}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(1, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterDummyDataTest, GetDownSampledData4) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"*\"],"
    "\"numOfPoints\":0}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(0, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterDummyDataTest, GetDownSampledData5) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"*\"],"
    "\"numOfPoints\":-1}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(0, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

TEST_F(GraphFilterDummyDataTest, GetDownSampledData6) {
  std::string result = gf.getData("{\"startDate\":\"2015-03-03 00:00Z\","
    "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"*\"]}");
  Json::Reader reader;
  Json::Value json_root;
  Json::Value json_element;
  ASSERT_TRUE(reader.parse(result, json_root));
  ASSERT_TRUE(json_root.isMember("points"));
  EXPECT_GE(1440, json_root["points"].size()) << " result size: " << json_root["points"].size();
}

/********************************************************************
* DatabaseAccess tests
********************************************************************/

/*
* Init tests
*/
// Check clean=false fails, clean=true passes
TEST_F(DatabaseAccessTest, Init) {
  EXPECT_TRUE(da.init(database_path,data_schema_json_, true));
}

/*
* Put Tests
*/

TEST_F(DatabaseAccessTest, PutEmptyData) {
  EXPECT_TRUE(da.init(database_path,data_schema_json_, true));
  EXPECT_FALSE(da.putData(Json::Value(Json::objectValue)));
}

TEST_F(DatabaseAccessTest, PutMalformedData) {
  EXPECT_TRUE(da.init(database_path,data_schema_json_, true));
  EXPECT_FALSE(da.putData(Json::Value(Json::nullValue)));
}

TEST_F(DatabaseAccessTest, AddSingleData) {
  EXPECT_TRUE(da.init(database_path,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  Json::Value param_json;
  ASSERT_TRUE(reader_.parse(param, param_json));
  EXPECT_TRUE(da.putData(param_json)) << " input param: " << param;
}

// DatabaseAccess ignores data it already contains
TEST_F(DatabaseAccessTest, AddDuplicateData) {
  EXPECT_TRUE(da.init(database_path,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  Json::Value param_json;
  ASSERT_TRUE(reader_.parse(param, param_json));
  EXPECT_TRUE(da.putData(param_json)) << " input param: " << param;
  EXPECT_TRUE(da.putData(param_json)) << " input param: " << param;
}


/*
* Get Tests
*/

TEST_F(DatabaseAccessTest, GetEmptyQuery) {
  EXPECT_TRUE(da.init(database_path,data_schema_json_, true));
  Json::Value result = da.getData(Json::Value(Json::objectValue));

  std::string empty = "{\"startDate\":\"\",\"endDate\":\"\",\"points\":[]}";
  Json::Value json_root_empty;
  ASSERT_TRUE(reader_.parse(empty, json_root_empty));

  ASSERT_EQ(0,result.compare(json_root_empty));
}

TEST_F(DatabaseAccessTest, GetMalformedQuery) {
  EXPECT_TRUE(da.init(database_path,data_schema_json_, true));
  Json::Value result = da.getData(Json::nullValue);

  std::string empty = "{\"startDate\":\"\",\"endDate\":\"\",\"points\":[]}";
  Json::Value json_root_empty;
  ASSERT_TRUE(reader_.parse(empty, json_root_empty));

  ASSERT_EQ(0, result.compare(json_root_empty));
}

// Init the databaseaccess and check that no results are returned
TEST_F(DatabaseAccessTest, InitDatabaseAndGetData) {
  ASSERT_TRUE(da.init(database_path,data_schema_json_, true));
  std::string query = "{\"startDate\":\"2015-03-03 00:00Z\","
                       "\"endDate\":\"2015-03-03 23:59Z\"}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = da.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(0, result["points"].size()) << " result size: " << result["points"].size();
}

// Init the databaseaccess, put data in, and check that the results are returned
TEST_F(DatabaseAccessTest, InitDatabasePutAndGetData) {
  ASSERT_TRUE(da.init(database_path,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0},"
    "{\"date\":\"2015-03-03 00:10Z\","
     "\"calories\":1.5,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":62,"
     "\"body_temp\":88.8,"
     "\"steps\":10}]}";
  Json::Value json_root_param;
  ASSERT_TRUE(reader_.parse(param, json_root_param));
  ASSERT_TRUE(da.putData(json_root_param)) << " input param: " << param;

  std::string query = "{\"startDate\":\"2015-03-03 00:00Z\","
                       "\"endDate\":\"2015-03-03 23:59Z\"}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = da.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(2, result["points"].size()) << " result size: " << result["points"].size();

  //printf("json_root_param:\n%s\nresult:\n%s\n",json_root_param.toStyledString().c_str(),result.toStyledString().c_str());

  ASSERT_EQ(0,json_root_param.compare(result));
}


/********************************************************************
* DataCache tests
********************************************************************/

/*
* Init tests
*/
// Check clean=false fails, clean=true passes
TEST_F(DataCacheTest, Init) {
  EXPECT_FALSE(dc.init(cache_setup_json_,data_schema_json_, false));
  EXPECT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
}

TEST_F(DataCacheTest, InitNoDownsampling) {
  std::string cache_setup_tmp = "{\"cacheRawData\": true"
         "}";
  EXPECT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
}

TEST_F(DataCacheTest, InitNoRaw) {
  // Passing false
  std::string cache_setup_tmp = "{\"cacheRawData\": false,"
         "\"downsamplingLevels\": ["
         "   { \"duration\": 86400,"
         "     \"numOfPoints\": 100"
         "   },"
         "   { \"duration\": 86400,"
         "     \"numOfPoints\": 10000"
         "   }"
         " ]"
         "}";
  Json::Value cache_setup_tmp_json;
  reader_.parse(cache_setup_tmp, cache_setup_tmp_json);
  EXPECT_TRUE(dc.init(cache_setup_tmp_json,data_schema_json_, true));
  // Omitting value
  cache_setup_tmp = "{\"downsamplingLevels\": ["
           "   { \"duration\": 86400,"
           "     \"numOfPoints\": 100"
           "   },"
           "   { \"duration\": 86400,"
           "     \"numOfPoints\": 10000"
           "   }"
           " ]"
           "}";
    reader_.parse(cache_setup_tmp, cache_setup_tmp_json);
    EXPECT_TRUE(dc.init(cache_setup_tmp_json,data_schema_json_, true));
}

TEST_F(DataCacheTest, InitNoDownsamplingNoRaw) {
  std::string cache_setup_tmp = "{}";
  Json::Value cache_setup_tmp_json;
  reader_.parse(cache_setup_tmp, cache_setup_tmp_json);
  EXPECT_TRUE(dc.init(cache_setup_tmp_json,data_schema_json_, true));
}

TEST_F(DataCacheTest, Singleton) {
  intel::poc::DataCache& dc2 = intel::poc::SQLiteDataCache::instance();
  ASSERT_TRUE(&dc == &dc2);
}

/*
* Put Tests
*/

TEST_F(DataCacheTest, CacheEmptyDates) {
  ASSERT_THROW(dc.cacheData("",""),std::runtime_error);
}

TEST_F(DataCacheTest, AddSingleData) {
  ASSERT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  Json::Value param_json;
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));
}

TEST_F(DataCacheTest, AddMultipleDataPointsAtSameTime) {
  ASSERT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0},"
    "{\"date\":\"2015-03-03 00:10Z\","
     "\"calories\":1.5,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":62,"
     "\"body_temp\":88.8,"
     "\"steps\":10}]}";
  Json::Value param_json;
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));
}

// The time intervals for the second set of data overlaps first set on the left and should be merged
TEST_F(DataCacheTest, AddDataOverlapsLeft) {
  ASSERT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 12:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 13:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  Json::Value param_json;
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 12:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 14:00Z\","
     "\"points\" : [{\"date\":\"2015-03-03 01:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 14:00Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  // Check to see if the data is retrievable
  std::string query = "{\"startDate\":\"2015-03-03 00:00Z\","
                       "\"endDate\":\"2015-03-03 23:59Z\","
                       "\"numOfPoints\":1000}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(2, result["points"].size()) << " result size: " << result["points"].size();
}

// The time intervals for the second set of data overlaps first set on the right and should be merged
TEST_F(DataCacheTest, AddDataOverlapsRight) {
  ASSERT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 14:00Z\","
     "\"points\" : [{\"date\":\"2015-03-03 01:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  Json::Value param_json;
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 14:00Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  param = "{\"startDate\":\"2015-03-03 12:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 15:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 12:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  // Check to see if the data is retrievable
  std::string query = "{\"startDate\":\"2015-03-03 00:00Z\","
                       "\"endDate\":\"2015-03-03 23:59Z\","
                       "\"numOfPoints\":1000}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(2, result["points"].size()) << " result size: " << result["points"].size();
}

// The time intervals for the second set of data does not overlap the first set
// and is entirely on the right. No merging should occur.
TEST_F(DataCacheTest, AddDataDoesNotOverlapsRight) {
  ASSERT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 11:00Z\","
     "\"points\" : [{\"date\":\"2015-03-03 01:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  Json::Value param_json;
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 11:00Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  param = "{\"startDate\":\"2015-03-03 12:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 13:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 12:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  // Check to see if the data is retrievable
  std::string query = "{\"startDate\":\"2015-03-03 12:30Z\","
                       "\"endDate\":\"2015-03-03 23:59Z\","
                       "\"numOfPoints\":1000}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(1, result["points"].size()) << " result size: " << result["points"].size();

  // Check sure you CAN'T retrieve data that bridges the gap in the two sets
  query = "{\"startDate\":\"2015-03-03 00:30Z\","
           "\"endDate\":\"2015-03-03 23:59Z\","
           "\"numOfPoints\":1000}";
  query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(0, result["points"].size()) << " result size: " << result["points"].size();
}

// The time intervals for the second set of data does not overlap the first set
// and is entirely on the left. No merging should occur.
TEST_F(DataCacheTest, AddDataDoesNotOverlapsLeft) {
  ASSERT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 12:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 13:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  Json::Value param_json;
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 12:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 11:00Z\","
     "\"points\" : [{\"date\":\"2015-03-03 01:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 11:00Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  // Check to see if the data is retrievable
  std::string query = "{\"startDate\":\"2015-03-03 12:30Z\","
                       "\"endDate\":\"2015-03-03 23:59Z\","
                       "\"numOfPoints\":1000}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(1, result["points"].size()) << " result size: " << result["points"].size();

  // Check sure you CAN'T retrieve data that bridges the gap in the two sets
  query = "{\"startDate\":\"2015-03-03 00:30Z\","
           "\"endDate\":\"2015-03-03 23:59Z\","
           "\"numOfPoints\":1000}";
  query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(0, result["points"].size()) << " result size: " << result["points"].size();
}



// Cache ignores data it already contains
TEST_F(DataCacheTest, AddDuplicateData) {
  ASSERT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}]}";
  Json::Value param_json;
  ASSERT_TRUE(reader_.parse(param, param_json));
  ASSERT_TRUE(da.putData(param_json)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));
}

/*
* Get Tests
*/

TEST_F(DataCacheTest, GetEmptyQuery) {
  Json::Value result = dc.getData(Json::Value(Json::objectValue));

  std::string empty = "{\"startDate\":\"\",\"endDate\":\"\",\"points\":[]}";
  Json::Value json_root_empty;
  ASSERT_TRUE(reader_.parse(empty, json_root_empty));

  ASSERT_EQ(0,result.compare(json_root_empty));
}

TEST_F(DataCacheTest, GetMalformedQuery) {
  Json::Value result = dc.getData(Json::Value(Json::nullValue));

  std::string empty = "{\"startDate\":\"\",\"endDate\":\"\",\"points\":[]}";
  Json::Value json_root_empty;
  ASSERT_TRUE(reader_.parse(empty, json_root_empty));

  ASSERT_EQ(0, result.compare(json_root_empty));
}

// Init the cache and check that no results are returned
TEST_F(DataCacheTest, InitDatabaseAndGetData) {
  EXPECT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string query = "{\"startDate\":\"2015-03-03 00:00Z\","
                       "\"endDate\":\"2015-03-03 23:59Z\","
                       "\"numOfPoints\":1000}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(0, result["points"].size()) << " result size: " << result["points"].size();
}

// Init the cache, put data in, and check that the results are returned
TEST_F(DataCacheTest, InitDatabasePutAndGetData) {
  ASSERT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0},"
    "{\"date\":\"2015-03-03 00:10Z\","
     "\"calories\":1.5,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":62,"
     "\"body_temp\":88.8,"
     "\"steps\":10}]}";


  Json::Value json_root_param;
  ASSERT_TRUE(reader_.parse(param, json_root_param));
  ASSERT_TRUE(da.putData(json_root_param)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  std::string query = "{\"startDate\":\"2015-03-03 00:00Z\","
                       "\"endDate\":\"2015-03-03 23:59Z\","
                       "\"numOfPoints\":1000}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(2, result["points"].size()) << " result size: " << result["points"].size();

  //printf("json_root_param:\n%s\nresult:\n%s\n",json_root_param.toStyledString().c_str(),result.toStyledString().c_str());

  ASSERT_EQ(0,json_root_param.compare(result));
}


// Init the cache, put data in, request only one metric, and check results are valid
TEST_F(DataCacheTest, InitDatabasePutAndGetOnlyGSRtData) {
  ASSERT_TRUE(dc.init(cache_setup_json_,data_schema_json_, true));
  std::string param = "{\"startDate\":\"2015-03-03 00:00Z\","
     "\"endDate\":\"2015-03-03 23:59Z\","
     "\"points\" : [{\"date\":\"2015-03-03 00:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0},"
    "{\"date\":\"2015-03-03 00:10Z\","
     "\"calories\":1.5,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":62,"
     "\"body_temp\":88.8,"
     "\"steps\":10}]}";

  Json::Value json_root_param;
  ASSERT_TRUE(reader_.parse(param, json_root_param));
  ASSERT_TRUE(da.putData(json_root_param)) << " input param: " << param;

  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-03 23:59Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  std::string query = "{\"startDate\":\"2015-03-03 00:00Z\","
                       "\"endDate\":\"2015-03-03 23:59Z\",\"metrics\":[\"gsr\"],"
                       "\"numOfPoints\":1000}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(2, result["points"].size()) << " result size: " << result["points"].size();

  //printf("json_root_param:\n%s\nresult:\n%s\n",json_root_param.toStyledString().c_str(),result.toStyledString().c_str());

  EXPECT_TRUE(!result["points"][0].isMember("heart_rate"));
  EXPECT_TRUE(!result["points"][0].isMember("body_temp"));
  EXPECT_TRUE(!result["points"][0].isMember("calories"));
  EXPECT_TRUE(!result["points"][0].isMember("steps"));
  EXPECT_TRUE(result["points"][0].isMember("gsr"));
}

// Init the cache, put data in, request only one metric, and check results are valid
TEST_F(DataCacheTest, InitDatabasePutPrefetchAndGetBeforeAfter) {
  // Setup cache to pre-fetch 1 interval before and after
  std::string cache_setup_tmp = "{\"cacheRawData\": true,"
     "\"fetchAhead\": 1,"
     "\"fetchBehind\": 1,"
     "\"downsamplingLevels\": ["
     "   { \"duration\": 86400,"
     "     \"numOfPoints\": 100"
     "   },"
     "   { \"duration\": 86400,"
     "     \"numOfPoints\": 1000"
     "   }"
     " ]"
     "}";
  Json::Value cache_setup_tmp_json;
  reader_.parse(cache_setup_tmp, cache_setup_tmp_json);
  ASSERT_TRUE(dc.init(cache_setup_tmp_json,data_schema_json_, true));
  // Put data from 2015-03-02 00:00 to 2015-03-05 00:00 into database
  std::string param = "{\"startDate\":\"2015-03-02 00:00Z\","
     "\"endDate\":\"2015-03-05 00:00Z\","
     "\"points\" : [{\"date\":\"2015-03-02 01:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0},"
    "{\"date\":\"2015-03-03 01:00Z\","
     "\"calories\":1.5,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":62,"
     "\"body_temp\":88.8,"
     "\"steps\":10},"
    "{\"date\":\"2015-03-04 01:00Z\","
     "\"calories\":1.4,"
     "\"gsr\":5.12886e-05,"
     "\"heart_rate\":61,"
     "\"body_temp\":88.7,"
     "\"steps\":0}"
    "]}";

  Json::Value json_root_param;
  ASSERT_TRUE(reader_.parse(param, json_root_param));
  ASSERT_TRUE(da.putData(json_root_param)) << " input param: " << param;

  // Only call cacheData for 2015-03-03 00:00 to 2015-03-04 00:00
  EXPECT_NO_THROW(dc.cacheData("2015-03-03 00:00Z","2015-03-04 00:00Z"));

  // Sleep for some time to give it time to async put
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_));

  // Get data from 2015-03-02 00:00 to 2015-03-05 00:00
  std::string query = "{\"startDate\":\"2015-03-02 00:00Z\","
                       "\"endDate\":\"2015-03-05 00:00Z\","
                       "\"numOfPoints\":1000}";
  Json::Value query_json;
  ASSERT_TRUE(reader_.parse(query, query_json));
  Json::Value result = dc.getData(query_json);
  ASSERT_TRUE(result.isMember("points"));
  ASSERT_EQ(3, result["points"].size()) << " result size: " << result["points"].size();

  //printf("json_root_param:\n%s\nresult:\n%s\n",json_root_param.toStyledString().c_str(),result.toStyledString().c_str());
}

int main(int argc, char * argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
