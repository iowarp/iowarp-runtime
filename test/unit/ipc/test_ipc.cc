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
#include "bdev/bdev_client.h"
#include "chimaera/api/chimaera_client.h"
#include "chimaera_admin/chimaera_admin_client.h"
#include "omp.h"
#include "small_message/small_message_client.h"

CHI_NAMESPACE_INIT

TEST_CASE("TestIpc") {
  CHIMAERA_CLIENT_INIT();

  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::small_message::Client client;
  CHI_ADMIN->RegisterModule(HSHM_MCTX, chi::DomainQuery::GetGlobalBcast(),
                            "chimaera_small_message");
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  hshm::Timer t;
  size_t domain_size = CHI_ADMIN->GetDomainSize(
      HSHM_MCTX, chi::DomainQuery::GetLocalHash(0),
      chi::DomainId(client.id_, chi::SubDomain::kGlobalContainers));

  size_t ops = 256;
  HILOG(kInfo, "OPS: {}", ops);
  t.Resume();
  int depth = 0;
  for (size_t i = 0; i < ops; ++i) {
    int cont_id = i;
    int ret = client.Md(HSHM_MCTX,
                        chi::DomainQuery::GetDirectHash(
                            chi::SubDomain::kGlobalContainers, cont_id),
                        depth, 0);
    REQUIRE(ret == 1);
  }
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps, {} MTasks", ops / t.GetUsec(),
        ops * (depth + 1) / t.GetUsec());
}

TEST_CASE("TestAsyncIpc") {
  CHIMAERA_CLIENT_INIT();

  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::small_message::Client client;
  CHI_ADMIN->RegisterModule(HSHM_MCTX, chi::DomainQuery::GetGlobalBcast(),
                            "chimaera_small_message");
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::Timer t;
  size_t domain_size = CHI_ADMIN->GetDomainSize(
      HSHM_MCTX, chi::DomainQuery::GetLocalHash(0),
      chi::DomainId(client.id_, chi::SubDomain::kGlobalContainers));

  int pid = HSHM_SYSTEM_INFO->pid_;
  hshm::ProcessAffiner::SetCpuAffinity(pid, 8);

  t.Resume();
  int depth = 8;
  size_t ops = (1 << 20);
  for (size_t i = 0; i < ops; ++i) {
    int ret;
    // HILOG(kInfo, "Sending message {}", i);
    int cont_id = i;
    client.AsyncMd(HSHM_MCTX,
                   chi::DomainQuery::GetDirectHash(
                       chi::SubDomain::kGlobalContainers, cont_id),
                   depth, TASK_FIRE_AND_FORGET);
  }
  CHI_ADMIN->Flush(HSHM_MCTX, DomainQuery::GetLocalHash(0));
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps, {} MTasks", ops / t.GetUsec(),
        ops * (depth + 1) / t.GetUsec());
}

TEST_CASE("TestFlush") {
  CHIMAERA_CLIENT_INIT();

  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::small_message::Client client;
  CHI_ADMIN->RegisterModule(HSHM_MCTX, chi::DomainQuery::GetGlobalBcast(),
                            "chimaera_small_message");
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::Timer t;

  int pid = HSHM_SYSTEM_INFO->pid_;
  hshm::ProcessAffiner::SetCpuAffinity(pid, 8);

  t.Resume();
  size_t ops = 256;
  for (size_t i = 0; i < ops; ++i) {
    int ret;
    HILOG(kInfo, "Sending message {}", i);
    int cont_id = 1 + ((i + 1) % nprocs);
    FullPtr<chi::small_message::MdTask> task =
        client.AsyncMd(HSHM_MCTX,
                       chi::DomainQuery::GetDirectHash(
                           chi::SubDomain::kGlobalContainers, cont_id),
                       0, 0);
  }
  CHI_ADMIN->Flush(HSHM_MCTX, DomainQuery::GetGlobalBcast());
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps", ops / t.GetUsec());
}

void TestIpcMultithread(int nprocs) {
  CHIMAERA_CLIENT_INIT();

  chi::small_message::Client client;
  CHI_ADMIN->RegisterModule(HSHM_MCTX, chi::DomainQuery::GetGlobalBcast(),
                            "chimaera_small_message");
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");

#pragma omp parallel shared(client, nprocs) num_threads(nprocs)
  {
    hshm::Timer t;
    t.Resume();
    int rank = omp_get_thread_num();
    size_t ops = (1 << 20);
    for (size_t i = 0; i < ops; ++i) {
      int ret;
      int cont_id = 1 + ((i + 1) % nprocs);
      ret = client.Md(HSHM_MCTX,
                      chi::DomainQuery::GetDirectHash(
                          chi::SubDomain::kGlobalContainers, cont_id),
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

TEST_CASE("TestIpcMultithread4") { TestIpcMultithread(4); }

TEST_CASE("TestIpcMultithread8") { TestIpcMultithread(8); }

TEST_CASE("TestIpcMultithread16") { TestIpcMultithread(16); }

TEST_CASE("TestIpcMultithread32") { TestIpcMultithread(32); }

void TestBulk(chi::IntFlag flags) {
  CHIMAERA_CLIENT_INIT();
  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::small_message::Client client;
  CHI_ADMIN->RegisterModule(HSHM_MCTX, chi::DomainQuery::GetGlobalBcast(),
                            "chimaera_small_message");
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  hshm::Timer t;

  int pid = HSHM_SYSTEM_INFO->pid_;
  hshm::ProcessAffiner::SetCpuAffinity(pid, 8);

  HILOG(kInfo, "Starting IO test: {}", nprocs);

  t.Resume();
  size_t ops = 1000;
  for (size_t i = 0; i < ops; ++i) {
    size_t write_ret = 0, read_ret = 0;
    HILOG(kInfo, "Sending message {}", i);
    int cont_id = i;
    client.Io(HSHM_MCTX,
              chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers,
                                              cont_id),
              KILOBYTES(4), flags, write_ret, read_ret);
    if (flags & MD_IO_WRITE) {
      REQUIRE(write_ret == KILOBYTES(4) * 10);
    }
    if (flags & MD_IO_READ) {
      REQUIRE(read_ret == KILOBYTES(4) * 15);
    }
  }
  t.Pause();

  HILOG(kInfo, "Latency: {} KOps", ops / t.GetMsec());
}

TEST_CASE("TestBulkWrite") { TestBulk(MD_IO_WRITE); }

TEST_CASE("TestBulkRead") { TestBulk(MD_IO_READ); }

TEST_CASE("TestBulk") { TestBulk(MD_IO_WRITE | MD_IO_READ); }

TEST_CASE("TestUpgrade") {
  CHIMAERA_CLIENT_INIT();

  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::small_message::Client client;
  CHI_ADMIN->RegisterModule(HSHM_MCTX, chi::DomainQuery::GetGlobalBcast(),
                            "chimaera_small_message");
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::Timer t;
  size_t domain_size = CHI_ADMIN->GetDomainSize(
      HSHM_MCTX, chi::DomainQuery::GetLocalHash(0),
      chi::DomainId(client.id_, chi::SubDomain::kGlobalContainers));

  int pid = getpid();
  hshm::ProcessAffiner::SetCpuAffinity(pid, 8);

  t.Resume();
  int depth = 4;
  size_t ops = 8192;
  for (size_t i = 0; i < ops / 2; ++i) {
    int ret;
    // HILOG(kInfo, "Sending message {}", i);
    int cont_id = i;
    client.AsyncMd(HSHM_MCTX,
                   chi::DomainQuery::GetDirectHash(
                       chi::SubDomain::kGlobalContainers, cont_id),
                   depth, TASK_FIRE_AND_FORGET);
  }
  CHI_ADMIN->AsyncUpgradeModule(HSHM_MCTX, chi::DomainQuery::GetGlobalBcast(),
                                "chimaera_small_message");
  for (size_t i = 0; i < ops / 2; ++i) {
    int ret;
    // HILOG(kInfo, "Sending message {}", i);
    int cont_id = i;
    client.AsyncMd(HSHM_MCTX,
                   chi::DomainQuery::GetDirectHash(
                       chi::SubDomain::kGlobalContainers, cont_id),
                   depth, TASK_FIRE_AND_FORGET);
  }

  CHI_ADMIN->Flush(HSHM_MCTX, DomainQuery::GetLocalHash(0));
  t.Pause();

  HILOG(kInfo, "Latency: {} MOps, {} MTasks", ops / t.GetUsec(),
        ops * (depth + 1) / t.GetUsec());
}

void TestBdevIo(const std::string &path) {
  CHIMAERA_CLIENT_INIT();

  int rank, nprocs;
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  chi::bdev::Client client;
  CHI_ADMIN->RegisterModule(HSHM_MCTX, chi::DomainQuery::GetGlobalBcast(),
                            "chimaera_bdev");
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "tempdir",
      "fs::///tmp/chi_test_bdev.bin", GIGABYTES(1));
  MPI_Barrier(MPI_COMM_WORLD);
  hshm::Timer t;

  hipc::FullPtr<char> io_write =
      CHI_CLIENT->AllocateBuffer(HSHM_MCTX, MEGABYTES(1));
  hipc::FullPtr<char> io_read =
      CHI_CLIENT->AllocateBuffer(HSHM_MCTX, MEGABYTES(1));

  t.Resume();
  size_t ops = 16;
  for (size_t i = 0; i < ops; ++i) {
    int ret;
    // HILOG(kInfo, "Sending message {}", i);
    int cont_id = i;
    std::vector<chi::Block> blocks =
        client.Allocate(HSHM_MCTX,
                        chi::DomainQuery::GetDirectHash(
                            chi::SubDomain::kGlobalContainers, cont_id),
                        MEGABYTES(1));
    chi::Block block = blocks[0];
    // Write the data that was allocated
    memset(io_write.ptr_, 10, MEGABYTES(1));
    client.Write(HSHM_MCTX,
                 chi::DomainQuery::GetDirectHash(
                     chi::SubDomain::kGlobalContainers, cont_id),
                 io_write.shm_, block.off_, MEGABYTES(1));
    // Read the data that was written
    memset(io_read.ptr_, 0, MEGABYTES(1));
    client.Read(HSHM_MCTX,
                chi::DomainQuery::GetDirectHash(
                    chi::SubDomain::kGlobalContainers, cont_id),
                io_read.shm_, block.off_, MEGABYTES(1));
    // Verify the correctness of data
    REQUIRE(memcmp(io_write.ptr_, io_read.ptr_, MEGABYTES(1)) == 0);
    // Free the buffer
    client.Free(HSHM_MCTX,
                chi::DomainQuery::GetDirectHash(
                    chi::SubDomain::kGlobalContainers, cont_id),
                block);
    // Poll the stats
    chi::BdevStats stats = client.PollStats(
        HSHM_MCTX, chi::DomainQuery::GetDirectHash(
                       chi::SubDomain::kGlobalContainers, cont_id));
    HILOG(kInfo, "Stats: {}", stats);
  }
  t.Pause();

  CHI_CLIENT->FreeBuffer(HSHM_MCTX, io_write);
  CHI_CLIENT->FreeBuffer(HSHM_MCTX, io_read);
}

TEST_CASE("TestBdevIo") { TestBdevIo("fs::///tmp/chi_test_bdev.bin"); }

TEST_CASE("TestBdevRam") { TestBdevIo("ram:://"); }

#ifdef CHIMAERA_ENABLE_PYTHON

#include "chimaera/monitor/monitor.h"

TEST_CASE("TestPython") {
  chi::PythonWrapper python;
  chi::LeastSquares ls;
  ls.Shape("test_python", 1, 1, 1, "SmallMessage.monitor_io");
  for (int i = 0; i < 100; ++i) {
    ls.Add({(float)i, (float)i + 1}, chi::Load());
  }
  python.RegisterPath("/home/llogan/Documents/Projects/chimaera_codegen/src");
  python.RegisterPath(
      "/home/llogan/Documents/Projects/chimaera_codegen/tasks/small_message/"
      "src");
  python.ImportModule("chimaera_monitor");
  python.ImportModule("small_message_monitor");
  if (ls.DoTrain()) {
    python.RunMethod<chi::LeastSquares>("ChimaeraMonitor", "least_squares_fit",
                                        ls);
  }
  HILOG(kInfo, "Slope: {}", ls.consts_[0]);
}
#endif
