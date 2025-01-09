/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <hermes_shm/util/affinity.h>
#include <hermes_shm/util/timer.h>
#include <mpi.h>

#include "basic_test.h"
#include "chimaera/api/chimaera_client.h"
#include "chimaera_admin/chimaera_admin.h"
#include "omp.h"
#include "small_message/small_message.h"

TEST_CASE("TestTypeSizes") {
  HILOG(kInfo, "Size of int: {}", sizeof(int));
  HILOG(kInfo, "Size of long: {}", sizeof(long));
  HILOG(kInfo, "Size of long long: {}", sizeof(long long));
  HILOG(kInfo, "Size of float: {}", sizeof(float));
  HILOG(kInfo, "Size of double: {}", sizeof(double));

  HILOG(kInfo, "Size of PoolId: {}", sizeof(chi::PoolId));
  HILOG(kInfo, "Size of TaskId: {}", sizeof(chi::TaskId));
  HILOG(kInfo, "Size of QueueId: {}", sizeof(chi::QueueId));
  HILOG(kInfo, "Size of DomainId: {}", sizeof(chi::DomainId));
  HILOG(kInfo, "Size of SubDomainId: {}", sizeof(chi::SubDomainId));
  HILOG(kInfo, "Size of DomainId: {}", sizeof(chi::DomainId));
  HILOG(kInfo, "Size of DomainSelection: {}", sizeof(chi::DomainSelection));
  HILOG(kInfo, "Size of DomainQuery: {}", sizeof(chi::DomainQuery));
  HILOG(kInfo, "Size of DomainQuery: {}", sizeof(chi::DomainQuery));
  HILOG(kInfo, "Size of RunContext: {}", sizeof(chi::RunContext));
  HILOG(kInfo, "Size of Task: {}", sizeof(chi::Task));
}