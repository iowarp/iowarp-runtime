//
// Created by llogan on 4/4/24.
//

#include <mpi.h>
#include "chimaera/api/chimaera_client.h"
#include "chimaera_admin/chimaera_admin.h"

#include "small_message/small_message.h"
#include "hermes_shm/util/timer.h"
#include "hermes_shm/util/timer_mpi.h"
#include "chimaera/work_orchestrator/affinity.h"
#include "omp.h"

void Summarize(size_t nprocs,
               double time_usec,
               size_t ops_per_node, size_t depth) {
  size_t ops = ops_per_node * nprocs;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0) {
    HILOG(kInfo, "Latency: {} MOps, {} MTasks, {} nprocs, {} ops-per-node",
          ops / time_usec,
          ops * (depth + 1) / time_usec,
          nprocs, ops_per_node);
  }
}

void SyncIpcTest(int rank, int nprocs, int depth, size_t ops) {
  chm::small_message::Client client;
  CHI_ADMIN->RegisterTaskLibRoot(
      chm::DomainQuery::GetLaneGlobalBcast(), "small_message");
  client.CreateRoot(
      chm::DomainQuery::GetDirectHash(chm::SubDomainId::kGlobalLaneSet, 0),
      chm::DomainQuery::GetLaneGlobalBcast(),
      "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::MpiTimer t(MPI_COMM_WORLD);
  size_t domain_size = CHI_ADMIN->GetDomainSizeRoot(
      chm::DomainQuery::GetDirectHash(chm::SubDomainId::kLocalLaneSet, 0),
      chm::DomainId(client.id_, chm::SubDomainId::kGlobalLaneSet));

  HILOG(kInfo, "OPS: {}", ops)
  t.Resume();
  for (size_t i = 0; i < ops; ++i) {
    int lane_id = 1 + (i % domain_size);
    client.MdRoot(
        chm::DomainQuery::GetDirectHash(chm::SubDomainId::kGlobalLaneSet, lane_id),
        depth, 0);
  }
  t.Pause();
  t.Collect();
  Summarize(nprocs, t.GetUsec(), ops, depth);
}

void AsyncIpcTest(int rank, int nprocs, int depth, size_t ops) {
  chm::small_message::Client client;
  CHI_ADMIN->RegisterTaskLibRoot(
      chm::DomainQuery::GetLaneGlobalBcast(),
      "small_message");
  client.CreateRoot(
      chm::DomainQuery::GetDirectHash(chm::SubDomainId::kGlobalLaneSet, 0),
      chm::DomainQuery::GetLaneGlobalBcast(),
      "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::MpiTimer t(MPI_COMM_WORLD);
  size_t domain_size = CHI_ADMIN->GetDomainSizeRoot(
      chm::DomainQuery::GetDirectHash(chm::SubDomainId::kLocalLaneSet, 0),
      chm::DomainId(client.id_, chm::SubDomainId::kGlobalLaneSet));

  t.Resume();
  for (size_t i = 0; i < ops; ++i) {
    HILOG(kDebug, "Sending message {}", i)
    int lane_id = 1 + (i % domain_size);
    client.AsyncMdRoot(
        chm::DomainQuery::GetDirectHash(chm::SubDomainId::kGlobalLaneSet, lane_id),
        depth, TASK_FIRE_AND_FORGET);
  }
  CHI_ADMIN->FlushRoot(
      DomainQuery::GetDirectHash(chm::SubDomainId::kLocalLaneSet, 0));
  t.Pause();
  t.Collect();
  Summarize(nprocs, t.GetUsec(), ops, depth);
}

int main(int argc, char **argv) {
  int rank, nprocs;
  MPI_Init(&argc, &argv);
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  CHIMAERA_CLIENT_INIT();

  if (argc < 3) {
    HILOG(kFatal, "Usage: test_ipc <depth> <ops> <async>");
    return 1;
  }

  int depth = std::stoi(argv[1]);
  size_t ops = hshm::ConfigParse::ParseSize(argv[2]);
  bool async = std::stoi(argv[3]);
  if (async) {
    AsyncIpcTest(rank, nprocs, depth, ops);
  } else {
    SyncIpcTest(rank, nprocs, depth, ops);
  }

  MPI_Finalize();
}