#include <bdev/bdev_client.h>
#include <mpi.h>

class IoTest {
 public:
  int xfer_;
  int block_;
  bool read_;

 public:
  void TestWrite() {}

  void TestRead() {}
}

int main(int argc, char **argv) {
  int rank, nprocs;
  MPI_Init(&argc, &argv);
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  CHIMAERA_CLIENT_INIT();

  if (argc != 4) {
    HILOG(kInfo, "USAGE: ./test_bdev_io [xfer] [block] [do_read (0/1)]");
  }

  IoTest test;
  test.xfer_ = hshm::ConfigParse::ParseSize(argv[1]);
  test.block_ = hshm::ConfigParse::ParseSize(argv[2]);
  test.read_ = atoi(argv[3]);

  test.TestWrite();
  MPI_Barrier(MPI_COMM_WORLD);
  if (test.read_) {
    test.TestRead();
  }
  MPI_Finalize();
}