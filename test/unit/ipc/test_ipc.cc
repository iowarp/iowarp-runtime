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

#include "basic_test.h"
#include <mpi.h>
#include "chimaera/api/chimaera_client.h"
#include "chimaera_admin/chimaera_admin.h"

#include "small_message/small_message.h"
#include "hermes_shm/util/timer.h"
#include "chimaera/work_orchestrator/affinity.h"
#include "omp.h"

TEST_CASE("TestIpc") {
  CHIMAERA_CLIENT_INIT();

  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::small_message::Client client;
  CHI_ADMIN->RegisterTaskLibRoot(
      chi::DomainQuery::GetGlobalBcast(),
      "small_message");
  client.CreateRoot(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(),
      "ipc_test");
  hshm::Timer t;
  size_t domain_size = CHI_ADMIN->GetDomainSizeRoot(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      chi::DomainId(client.id_, chi::SubDomainId::kGlobalContainers));

  size_t ops = 256;
  HILOG(kInfo, "OPS: {}", ops)
  t.Resume();
  int depth = 0;
  for (size_t i = 0; i < ops; ++i) {
    HILOG(kDebug, "Sending message {}", i);
    int lane_id = i;
    int ret = client.MdRoot(
        chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, lane_id),
        depth, 0);
    REQUIRE(ret == 1);
  }
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps, {} MTasks",
        ops / t.GetUsec(),
        ops * (depth + 1) / t.GetUsec());
}

TEST_CASE("TestAsyncIpc") {
  CHIMAERA_CLIENT_INIT();

  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::small_message::Client client;
  CHI_ADMIN->RegisterTaskLibRoot(
      chi::DomainQuery::GetGlobalBcast(), "small_message");
  client.CreateRoot(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(),
      "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::Timer t;
  size_t domain_size = CHI_ADMIN->GetDomainSizeRoot(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      chi::DomainId(client.id_, chi::SubDomainId::kGlobalContainers));

  int pid = getpid();
  ProcessAffiner::SetCpuAffinity(pid, 8);

  t.Resume();
  int depth = 8;
  size_t ops = (1 << 20);
  for (size_t i = 0; i < ops; ++i) {
    int ret;
    // HILOG(kInfo, "Sending message {}", i);
    int lane_id = i;
    client.AsyncMdRoot(
        chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, lane_id),
        depth, TASK_FIRE_AND_FORGET);
  }
  CHI_ADMIN->FlushRoot(
      DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0));
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps, {} MTasks",
        ops / t.GetUsec(),
        ops * (depth + 1) / t.GetUsec());
}

TEST_CASE("TestFlush") {
  CHIMAERA_CLIENT_INIT();

  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::small_message::Client client;
  CHI_ADMIN->RegisterTaskLibRoot(chi::DomainQuery::GetGlobalBcast(), "small_message");
  client.CreateRoot(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(),
      "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::Timer t;

  int pid = getpid();
  ProcessAffiner::SetCpuAffinity(pid, 8);

  t.Resume();
  size_t ops = 256;
  for (size_t i = 0; i < ops; ++i) {
    int ret;
    HILOG(kInfo, "Sending message {}", i);
    int lane_id = 1 + ((i + 1) % nprocs);
    LPointer<chi::small_message::MdTask> task = client.AsyncMdRoot(
        chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, lane_id),
        0, 0);
  }
  CHI_ADMIN->FlushRoot(DomainQuery::GetGlobalBcast());
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps", ops / t.GetUsec());
}

void TestIpcMultithread(int nprocs) {
  CHIMAERA_CLIENT_INIT();

  chi::small_message::Client client;
  CHI_ADMIN->RegisterTaskLibRoot(chi::DomainQuery::GetGlobalBcast(), "small_message");
  client.CreateRoot(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(),
      "ipc_test");

#pragma omp parallel shared(client, nprocs) num_threads(nprocs)
  {
    hshm::Timer t;
    t.Resume();
    int rank = omp_get_thread_num();
    size_t ops = (1 << 20);
    for (size_t i = 0; i < ops; ++i) {
      int ret;
      int lane_id = 1 + ((i + 1) % nprocs);
      ret = client.MdRoot(
          chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, lane_id),
          0, 0);
      REQUIRE(ret == 1);
    }
#pragma omp barrier
    t.Pause();
    if (rank == 0) {
      HILOG(kInfo, "Latency: {} MOps", ops * nprocs / t.GetUsec());
    }
  }

}

TEST_CASE("TestIpcMultithread4") {
  TestIpcMultithread(4);
}

TEST_CASE("TestIpcMultithread8") {
  TestIpcMultithread(8);
}

TEST_CASE("TestIpcMultithread16") {
  TestIpcMultithread(16);
}

TEST_CASE("TestIpcMultithread32") {
  TestIpcMultithread(32);
}

TEST_CASE("TestIO") {
  CHIMAERA_CLIENT_INIT();
  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::small_message::Client client;
  CHI_ADMIN->RegisterTaskLibRoot(chi::DomainQuery::GetGlobalBcast(), "small_message");
  client.CreateRoot(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(),
      "ipc_test");
  hshm::Timer t;
  size_t domain_size = CHI_ADMIN->GetDomainSizeRoot(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      chi::DomainId(client.id_, chi::SubDomainId::kGlobalContainers));

  int pid = getpid();
  ProcessAffiner::SetCpuAffinity(pid, 8);

  HILOG(kInfo, "Starting IO test: {}", nprocs);

  t.Resume();
  size_t ops = 16;
  for (size_t i = 0; i < ops; ++i) {
    size_t write_ret = 0, read_ret = 0;
    HILOG(kInfo, "Sending message {}", i);
    int lane_id = i;
    client.IoRoot(
        chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, lane_id),
        KILOBYTES(4),
        MD_IO_WRITE | MD_IO_READ,
        write_ret, read_ret);
    REQUIRE(write_ret == KILOBYTES(4) * 10);
    REQUIRE(read_ret == KILOBYTES(4) * 15);
  }
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps", ops / t.GetUsec());
}

// TEST_CASE("TestHostfile") {
//  for (NodeId lane_id = 1; node_id <
//  HRUN_THALLIUM->rpc_->hosts_.size() + 1; ++node_id) {
//    HILOG(kInfo, "Node {}: {}", node_id,
//    HRUN_THALLIUM->GetServerName(node_id));
//  }
// }
