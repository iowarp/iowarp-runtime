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
  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chm::small_message::Client client;
  CHM_ADMIN->RegisterTaskLibRoot(chm::DomainId::GetGlobal(), "small_message");
  client.CreateRoot(chm::DomainId::GetGlobal(), "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::Timer t;

  int pid = getpid();
  ProcessAffiner::SetCpuAffinity(pid, 8);

  size_t ops = (1 << 20);
  HILOG(kInfo, "OPS: {}", ops)
  t.Resume();
  int depth = 0;
  for (size_t i = 0; i < ops; ++i) {
    int ret;
    // HILOG(kInfo, "Sending message {}", i);
    int node_id = 1 + ((rank + 1) % nprocs);
    ret = client.MdRoot(chm::DomainId::GetNode(node_id),
                        i, depth, 0);
//    auto task = client.AsyncMd(
//        HRUN_CLIENT->MakeTaskNodeId() + 1,
//        chm::DomainId::GetNode(node_id),
//        depth, 0);
//    task->Wait();
//    ret = task->ret_[0];
    REQUIRE(ret == 1);
  }
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps, {} MTasks",
        ops / t.GetUsec(),
        ops * (depth + 1) / t.GetUsec());
}

TEST_CASE("TestAsyncIpc") {
  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chm::small_message::Client client;
  CHM_ADMIN->RegisterTaskLibRoot(chm::DomainId::GetGlobal(), "small_message");
  client.CreateRoot(chm::DomainId::GetGlobal(), "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::Timer t;

  int pid = getpid();
  ProcessAffiner::SetCpuAffinity(pid, 8);

  t.Resume();
  int depth = 8;
  size_t ops = (1 << 20);
  for (size_t i = 0; i < ops; ++i) {
    int ret;
    // HILOG(kInfo, "Sending message {}", i);
    int node_id = 1 + ((rank + 1) % nprocs);
    client.AsyncMdRoot(chm::DomainId::GetNode(node_id),
                       i, depth, TASK_FIRE_AND_FORGET);
//    auto task = client.AsyncMd(
//        nullptr,
//        HRUN_CLIENT->MakeTaskNodeId() + 1,
//        chm::DomainId::GetNode(node_id),
//        depth, TASK_FIRE_AND_FORGET);
  }
  CHM_ADMIN->FlushRoot(DomainId::GetLocal());
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps, {} MTasks",
        ops / t.GetUsec(),
        ops * (depth + 1) / t.GetUsec());
}

TEST_CASE("TestFlush") {
  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chm::small_message::Client client;
  CHM_ADMIN->RegisterTaskLibRoot(chm::DomainId::GetGlobal(), "small_message");
  client.CreateRoot(chm::DomainId::GetGlobal(), "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::Timer t;

  int pid = getpid();
  ProcessAffiner::SetCpuAffinity(pid, 8);

  t.Resume();
  size_t ops = 256;
  for (size_t i = 0; i < ops; ++i) {
    int ret;
    HILOG(kInfo, "Sending message {}", i);
    int node_id = 1 + ((rank + 1) % nprocs);
    LPointer<chm::small_message::MdTask> task =
        client.AsyncMdRoot(
            chm::DomainId::GetNode(node_id),
            node_id, 0, 0);
  }
  CHM_ADMIN->FlushRoot(DomainId::GetGlobal());
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps", ops / t.GetUsec());
}

void TestIpcMultithread(int nprocs) {
  chm::small_message::Client client;
  CHM_ADMIN->RegisterTaskLibRoot(chm::DomainId::GetGlobal(), "small_message");
  client.CreateRoot(chm::DomainId::GetGlobal(), "ipc_test");

#pragma omp parallel shared(client, nprocs) num_threads(nprocs)
  {
    hshm::Timer t;
    t.Resume();
    int rank = omp_get_thread_num();
    size_t ops = (1 << 20);
    for (size_t i = 0; i < ops; ++i) {
      int ret;
      int node_id = 1 + ((rank + 1) % nprocs);
      ret = client.MdRoot(chm::DomainId::GetNode(node_id),
                          i, 0, 0);
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
  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chm::small_message::Client client;
  CHM_ADMIN->RegisterTaskLibRoot(chm::DomainId::GetGlobal(), "small_message");
  client.CreateRoot(chm::DomainId::GetGlobal(), "ipc_test");
  hshm::Timer t;

  int pid = getpid();
  ProcessAffiner::SetCpuAffinity(pid, 8);

  HILOG(kInfo, "Starting IO test: {}", nprocs);

  t.Resume();
  size_t ops = 16;
  for (size_t i = 0; i < ops; ++i) {
    int ret;
    HILOG(kInfo, "Sending message {}", i);
    int node_id = 1 + ((rank + 1) % nprocs);
    ret = client.IoRoot(chm::DomainId::GetNode(node_id));
    REQUIRE(ret == 1);
  }
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps", ops / t.GetUsec());
}

// TEST_CASE("TestHostfile") {
//  for (u32 node_id = 1; node_id <
//  HRUN_THALLIUM->rpc_->hosts_.size() + 1; ++node_id) {
//    HILOG(kInfo, "Node {}: {}", node_id,
//    HRUN_THALLIUM->GetServerName(node_id));
//  }
// }
