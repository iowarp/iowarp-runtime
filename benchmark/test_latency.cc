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

CHI_NAMESPACE_INIT

void Summarize(size_t nprocs,
               double time_usec,
               size_t ops_per_node, size_t depth) {
  size_t ops = ops_per_node * nprocs;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0) {
    HILOG(kInfo, "Latency: {} KOps, {} KTasks, {} nprocs, {} ops-per-node",
          1000 * ops / time_usec,
          1000 * ops * (depth + 1) / time_usec,
          nprocs, ops_per_node);
  }
}

void SyncIpcTest(int rank, int nprocs, int depth, size_t ops) {
  chi::small_message::Client client;
  CHI_ADMIN->RegisterModule(
      chi::DomainQuery::GetGlobalBcast(), "small_message");
  client.Create(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(),
      "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::MpiTimer t(MPI_COMM_WORLD);

  HILOG(kInfo, "OPS: {}", ops)
  t.Resume();
  for (size_t i = 0; i < ops; ++i) {
    int container_id = i;
    client.Md(
        chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers,
                                        container_id),
        depth, 0);
  }
  t.Pause();
  t.Collect();
  Summarize(nprocs, t.GetUsec(), ops, depth);
}

void AsyncIpcTest(int rank, int nprocs, int depth, size_t ops) {
  chi::small_message::Client client;
  CHI_ADMIN->RegisterModule(
      chi::DomainQuery::GetGlobalBcast(),
      "small_message");
  client.Create(
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(),
      "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::MpiTimer t(MPI_COMM_WORLD);

  t.Resume();
  for (size_t i = 0; i < ops; ++i) {
    int container_id = i;
    client.AsyncMd(
        chi::DomainQuery::GetDirectHash(
            chi::SubDomainId::kGlobalContainers,
            container_id),
        depth, TASK_FIRE_AND_FORGET);
  }
  CHI_ADMIN->Flush(
      DomainQuery::GetGlobalBcast());
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
    HELOG(kFatal, "Usage: test_ipc <depth> <ops> <async>");
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