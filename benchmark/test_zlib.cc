//
// Created by llogan on 6/25/24.
//
#include <hermes_shm/util/config_parse.h>
#include "mpi.h"
#include "hermes_shm/util/compress/snappy.h"
#include "hermes_shm/util/random.h"

std::vector<int> MakeDist(size_t xfer_count) {
  // Create a normal distribution
  hshm::NormalDistribution normal;
  normal.Shape(0, 600);
  std::vector<int> data(xfer_count);
  normal.GetInt();
  for (size_t i = 0; i < xfer_count; ++i) {
    data[i] = normal.GetInt();
  }
  return data;
}

int main(int argc, char *argv[]) {
  MPI_Init(&argc, &argv);
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  size_t block = hshm::ConfigParse::ParseSize(argv[1]);
  size_t xfer = hshm::ConfigParse::ParseSize(argv[2]);
  bool do_compress = std::stoi(argv[3]);
  std::string basepath = argv[4];
  std::string path = basepath + "/" + std::to_string(rank);
  size_t xfer_count = xfer / sizeof(int);
  size_t iter = block / xfer;

  // Open a file stdio
  FILE *file = fopen(path.c_str(), "w");
  if (file == NULL) {
    fprintf(stderr, "Failed to open file\n");
    return 1;
  }
  std::vector<int> data = MakeDist(xfer_count);

  // Write compressed data repeatedly
  for (size_t i = 0; i < iter; ++i) {
    // Write the data
    size_t write_sz = 0;
    hshm::Timer t;
    t.Resume();
    if (do_compress) {
      hshm::Snappy compress;
      size_t compressed_size = xfer;
      std::vector<int> compressed(xfer_count);
      compress.Compress(compressed.data(), compressed_size,
                        data.data(), xfer);
      fwrite(compressed.data(), sizeof(char), compressed_size, file);
      write_sz = compressed_size;
    } else {
      fwrite(data.data(), sizeof(char), xfer, file);
      write_sz = xfer;
    }
    fflush(file);
    t.Pause();
    // fseek(file, 0, SEEK_SET);
    HILOG(kInfo, "Wrote {} bytes in {} usec (compress={})",
          write_sz, t.GetUsec(), do_compress);
  }
  MPI_Finalize();
}