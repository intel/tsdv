/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#ifndef INTEL_POC_DATABASEGRAPHFILTER_H
#define INTEL_POC_DATABASEGRAPHFILTER_H

#include <map>
#include "graphfilter.h"
#include <graphfilter/datafilter.h>

namespace intel {
  namespace poc {

    class DatabaseGraphFilter : public GraphFilter
    {
     public:
      /// constructor
      DatabaseGraphFilter();

      /// destructor
      ~DatabaseGraphFilter();

      /// API
      const std::string id() const;

      bool init(const std::string& cache_setup,
                const std::string& dataSchema,
                const std::string& database_path,
                bool clean);

      bool addData(const std::string& data_values);

      std::string getData(const std::string& params);

     private:
      /// private API

      std::map<std::string, std::string> data_schema_;
      bool use_cache_;
      bool cache_raw_data_;
      DataFilter::FilterType downsampling_filter_;
    };
  }
}

#endif // INTEL_POC_DATABASEGRAPHFILTER_H
