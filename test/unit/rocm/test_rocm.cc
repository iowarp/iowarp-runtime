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

#include "bdev/bdev_client.h"
#include "chimaera/api/chimaera_client.h"
#include "chimaera/api/chimaera_client_defn.h"
#include "chimaera_admin/chimaera_admin_client.h"
#include "omp.h"
#include "small_message/small_message_client.h"

HSHM_GPU_KERNEL void test_kernel() {
  // chi::TaskNode task_node = CHI_CLIENT->MakeTaskNodeId();
  // hipc::FullPtr<chi::Admin::RegisterModuleTask> task =
  //     CHI_ADMIN->AsyncRegisterModuleAlloc(HSHM_MCTX, task_node,
  //                                         chi::DomainQuery::GetGlobalBcast(),
  //                                         "small_message");
  // printf("H3: %p %p %p %p\n", task.ptr_, CHI_CLIENT, CHI_QM,
  //        CHI_QM->queue_map_);
  // chi::ingress::MultiQueue *queue =
  //     CHI_CLIENT->GetQueue(chi::PROCESS_QUEUE_ID);
  // printf("H4: (queue major, minor) %d.%d\n", queue->id_.group_id_,
  //        (int)queue->id_.unique_);
  // queue->Emplace(chi::TaskPrioOpt::kLowLatency,
  //                hshm::hash<chi::DomainQuery>{}(task->dom_query_),
  //                task.shm_);
  // printf("H5\n");
  CHI_ADMIN->RegisterModule(HSHM_MCTX, chi::DomainQuery::GetGlobalBcast(),
                            "small_message");
  // client.Create(
  //     HSHM_MCTX,
  //     chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers,
  //     0), chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  // hshm::Timer t;
  // size_t domain_size = CHI_ADMIN->GetDomainSize(
  //     HSHM_MCTX,
  //     chi::DomainQuery::GetLocalHash(0),
  //     chi::DomainId(client.id_, chi::SubDomainId::kGlobalContainers));

  // size_t ops = 256;
  // HILOG(kInfo, "OPS: {}", ops);
  // int depth = 0;
  // for (size_t i = 0; i < ops; ++i) {
  //   int cont_id = i;
  //   int ret = client.Md(HSHM_MCTX,
  //                       chi::DomainQuery::GetDirectHash(
  //                           chi::SubDomainId::kGlobalContainers, cont_id),
  //                       depth, 0);
  //   REQUIRE(ret == 1);

  printf("DONE!!!");
}

int main() {
  CHIMAERA_CLIENT_INIT();
  test_kernel<<<1, 1>>>();
  HIP_ERROR_CHECK(hipDeviceSynchronize());
}
