/*
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */

#include <graphfilter/databasegraphfilter.h>
#include <graphfilter/graphfilter.h>

namespace intel {
  namespace poc {

    GraphFilter& GraphFilter::instance()
    {
      static GraphFilter *instance = new DatabaseGraphFilter();

      return *instance;
    }
  }
}
