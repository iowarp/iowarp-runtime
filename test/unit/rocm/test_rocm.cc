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
#include "bdev/bdev.h"
#include "chimaera/api/chimaera_client.h"
#include "chimaera_admin/chimaera_admin.h"
#include "omp.h"
#include "small_message/small_message.h"

CHI_NAMESPACE_INIT

HSHM_GPU_KERNEL void test_kernel() {
  CHIMAERA_CLIENT_INIT();
  CHI_ADMIN->RegisterModule(HSHM_DEFAULT_MEM_CTX,
                            chi::DomainQuery::GetGlobalBcast(),
                            "small_message");
  client.Create(
      HSHM_DEFAULT_MEM_CTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  hshm::Timer t;
  size_t domain_size = CHI_ADMIN->GetDomainSize(
      HSHM_DEFAULT_MEM_CTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      chi::DomainId(client.id_, chi::SubDomainId::kGlobalContainers));

  size_t ops = 256;
  HILOG(kInfo, "OPS: {}", ops);
  int depth = 0;
  for (size_t i = 0; i < ops; ++i) {
    int cont_id = i;
    int ret = client.Md(HSHM_DEFAULT_MEM_CTX,
                        chi::DomainQuery::GetDirectHash(
                            chi::SubDomainId::kGlobalContainers, cont_id),
                        depth, 0);
    REQUIRE(ret == 1);
  }
}
