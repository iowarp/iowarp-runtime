#include <bdev/bdev_client.h>
#include <mpi.h>

class IoTest {
 public:
  chi::bdev::Client client_;
  std::string path_;
  int xfer_;
  int block_;
  bool read_;
  std::vector<chi::Block> blocks_;

 public:
  void TestWrite() {
    int node_id = 1;
    for (size_t io_done = 0; io_done < block_; io_done += xfer_) {
      chi::DomainQuery dom_query = chi::DomainQuery::GetDirectHash(
          chi::SubDomainId::kLocalContainers, node_id);
      std::vector<chi::Block> blocks =
          client_.Allocate(HSHM_MCTX, dom_query, xfer_);
      if (blocks.size() == 0) {
        HELOG(kFatal, "Not enough space for this workload on {}", path_);
      }
      chi::Block &block = blocks[0];
      hipc::FullPtr<char> data =
          CHI_CLIENT->AllocateBuffer(HSHM_MCTX, block.size_);
      memset(data.ptr_, node_id, block.size_);
      client_.Write(HSHM_MCTX, dom_query, data.shm_, block);
      blocks_.emplace_back(blocks[0]);
      CHI_CLIENT->FreeBuffer(HSHM_MCTX, data);
      node_id++;
    }
  }

  void TestRead() {
    int node_id = 1;
    for (chi::Block &block : blocks_) {
      chi::DomainQuery dom_query = chi::DomainQuery::GetDirectHash(
          chi::SubDomainId::kLocalContainers, node_id);
      hipc::FullPtr<char> data = CHI_CLIENT->AllocateBuffer(HSHM_MCTX, xfer_);
      client_.Read(HSHM_MCTX, dom_query, data.shm_, block);
      client_.Free(HSHM_MCTX, dom_query, block);
      CHI_CLIENT->FreeBuffer(HSHM_MCTX, data);
      node_id++;
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
  test.xfer_ = hshm::ConfigParse::ParseSize(argv[2]);
  test.block_ = hshm::ConfigParse::ParseSize(argv[3]);
  test.read_ = atoi(argv[4]);
  size_t net_size = test.block_ * nprocs * 2;

  test.client_.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "bdev_test", test.path_, net_size);
  test.TestWrite();
  MPI_Barrier(MPI_COMM_WORLD);
  if (test.read_) {
    test.TestRead();
  }
  MPI_Finalize();
}