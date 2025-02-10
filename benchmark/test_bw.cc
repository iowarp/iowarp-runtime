//
// Created by llogan on 4/4/24.
//

#include <hermes_shm/util/affinity.h>
#include <hermes_shm/util/timer.h>
#include <hermes_shm/util/timer_mpi.h>
#include <mpi.h>

#include "chimaera/api/chimaera_client.h"
#include "chimaera_admin/chimaera_admin.h"
#include "omp.h"
#include "small_message/small_message.h"

void Summarize(size_t nprocs, double time_usec, size_t ops_pp,
               size_t msg_size) {
  size_t ops = ops_pp * nprocs;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0) {
    HILOG(kInfo, "Performance: {} MOps, {} MBps, {} nprocs, {} ops-per-proc",
          ops / time_usec, msg_size * ops / time_usec, nprocs, ops_pp);
  }
}

void SyncIoTest(int rank, int nprocs, size_t msg_size, size_t ops_pp) {
  unsigned int cpu_id, numa;
  getcpu(&cpu_id, &numa);
  HILOG(kInfo, "I'm on CPU {}", cpu_id);

  chi::small_message::Client client;
  CHI_ADMIN->RegisterModule(HSHM_DEFAULT_MEM_CTX,
                            chi::DomainQuery::GetGlobalBcast(),
                            "small_message");
  client.Create(
      HSHM_DEFAULT_MEM_CTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  size_t domain_size = CHI_ADMIN->GetDomainSize(
      HSHM_DEFAULT_MEM_CTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
      chi::DomainId(client.id_, chi::SubDomainId::kGlobalContainers));

  hshm::MpiTimer t(MPI_COMM_WORLD);
  t.Resume();
  for (size_t i = 0; i < ops_pp; ++i) {
    int container_id = i;
    size_t read_size, write_size;
    client.Io(HSHM_DEFAULT_MEM_CTX,
              chi::DomainQuery::GetDirectHash(
                  chi::SubDomainId::kGlobalContainers, container_id),
              msg_size, MD_IO_WRITE, write_size, read_size);
  }
  t.Pause();
  t.Collect();
  Summarize(nprocs, t.GetUsec(), ops_pp, msg_size);
}

void AsyncIoTest(int rank, int nprocs, size_t msg_size, size_t ops_pp) {}

int main(int argc, char **argv) {
  int rank, nprocs;
  MPI_Init(&argc, &argv);
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  CHIMAERA_CLIENT_INIT();

  if (argc < 3) {
    HELOG(kFatal, "Usage: test_ipc <msg_size> <ops> <async>");
    return 1;
  }

  size_t msg_size = hshm::ConfigParse::ParseSize(argv[1]);
  size_t ops_pp = hshm::ConfigParse::ParseSize(argv[2]);
  bool async = std::stoi(argv[3]);
  if (async) {
    AsyncIoTest(rank, nprocs, msg_size, ops_pp);
  } else {
    SyncIoTest(rank, nprocs, msg_size, ops_pp);
  }

  MPI_Finalize();
}