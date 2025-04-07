#include <bdev/bdev_client.h>
#include <mpi.h>

#include "hermes_shm/util/timer_mpi.h"

class IoTest {
 public:
  chi::bdev::Client client_;
  std::string path_;
  size_t xfer_;
  size_t block_;
  size_t net_size_;
  size_t base_;
  bool read_;
  std::vector<chi::Block> blocks_;
  int rank_;
  CLS_CONST char key_ = 122;

 public:
  void TestWrite() {
    hshm::MpiTimer timer(MPI_COMM_WORLD);
    timer.Resume();
    int node_id = 1;
    for (size_t io_done = 0; io_done < block_; io_done += xfer_) {
      chi::DomainQuery dom_query = chi::DomainQuery::GetDirectHash(
          chi::SubDomain::kGlobalContainers, node_id);
      std::vector<chi::Block> blocks =
          client_.Allocate(HSHM_MCTX, dom_query, xfer_);
      if (blocks.size() == 0) {
        HELOG(kFatal, "Not enough space for this workload on {}", path_);
      }
      chi::Block &block = blocks[0];
      // chi::Block block;
      // block.off_ = base_ + io_done;
      // block.size_ = xfer_;
      hipc::FullPtr<char> data =
          CHI_CLIENT->AllocateBuffer(HSHM_MCTX, block.size_);
      HILOG(kInfo, "(rank {}) writing at offset  {} ({}%) ptr={}", rank_,
            io_done, io_done * 100.0 / (block_), (void *)data.ptr_);
      if (data.IsNull()) {
        HELOG(kFatal, "Buffer allocated was null");
      }
      memset(data.ptr_, key_, block.size_);
      client_.Write(HSHM_MCTX, dom_query, data.shm_, block);
      blocks_.emplace_back(block);
      CHI_CLIENT->FreeBuffer(HSHM_MCTX, data);
      node_id++;
    }
    timer.Pause();
    timer.Collect();
    if (rank_ == 0) {
      float mb = net_size_ * 1.0 / hshm::Unit<size_t>::Megabytes(1);
      float sec = timer.GetSec();
      float mbps = mb / sec;
      HILOG(kInfo,
            "{} MB / sec (total={} mb, total={} bytes, time={} sec) of data",
            mbps, mb, net_size_, sec);
    }
  }

  bool Verify(char *ptr, int id, size_t size) {
    for (int i = 0; i < size; ++i) {
      if (ptr[i] != id) {
        return false;
      }
    }
    return true;
  }

  size_t Sum(char *ptr, size_t size) {
    size_t sum = 0;
    for (int i = 0; i < size; ++i) {
      sum += ptr[i];
    }
    return sum;
  }

  void TestRead() {
    hshm::MpiTimer timer(MPI_COMM_WORLD);
    timer.Resume();
    int node_id = 1;
    for (chi::Block &block : blocks_) {
      chi::DomainQuery dom_query = chi::DomainQuery::GetDirectHash(
          chi::SubDomain::kGlobalContainers, node_id);
      hipc::FullPtr<char> data =
          CHI_CLIENT->AllocateBuffer(HSHM_MCTX, block.size_);
      HILOG(kInfo, "(rank {}) reading at offset {} ({}%) ptr={}", rank_,
            block.off_, (node_id - 1) * 100.0 / (blocks_.size()),
            (void *)data.ptr_);
      client_.Read(HSHM_MCTX, dom_query, data.shm_, block);
      if (data.IsNull()) {
        HELOG(kFatal, "Buffer allocated was null");
      }
      client_.Free(HSHM_MCTX, dom_query, block);
      if (!Verify(data.ptr_, key_, block.size_)) {
        size_t sum = Sum(data.ptr_, block.size_);
        HELOG(kFatal, "Read invalid: sum={} when expected {}", sum,
              block.size_ * key_);
      }
      CHI_CLIENT->FreeBuffer(HSHM_MCTX, data);
      node_id++;
    }
    timer.Pause();
    timer.Collect();
    if (rank_ == 0) {
      float mb = net_size_ * 1.0 / hshm::Unit<size_t>::Megabytes(1);
      float sec = timer.GetSec();
      float mbps = mb / sec;
      HILOG(kInfo,
            "{} MB / sec (total={} mb, total={} bytes, time={} sec) of data",
            mbps, mb, net_size_, sec);
    }
  }
};

int main(int argc, char **argv) {
  int rank, nprocs;
  MPI_Init(&argc, &argv);
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  CHIMAERA_CLIENT_INIT();

  if (argc != 5) {
    HILOG(kInfo, "USAGE: ./test_bdev_io [path] [xfer] [block] [do_read (0/1)]");
  }

  IoTest test;
  test.path_ = argv[1];
  test.rank_ = rank;
  test.xfer_ = hshm::ConfigParse::ParseSize(argv[2]);
  test.block_ = hshm::ConfigParse::ParseSize(argv[3]);
  test.base_ = test.block_ * test.rank_;
  test.read_ = atoi(argv[4]);
  test.net_size_ = test.block_ * (size_t)nprocs;
  if (rank == 0) {
    HILOG(kInfo,
          "TEST BEGIN: nprocs={} xfer={}, block={} total={}, block * nprocs={}",
          nprocs, test.xfer_, test.block_, test.net_size_,
          test.block_ * nprocs);
  }

  test.client_.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomain::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "bdev_test", test.path_,
      test.net_size_);
  test.TestWrite();
  MPI_Barrier(MPI_COMM_WORLD);
  if (test.read_) {
    test.TestRead();
  }
  MPI_Finalize();
}