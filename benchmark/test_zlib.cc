//
// Created by llogan on 6/25/24.
//
#include <hermes_shm/util/config_parse.h>
#include "mpi.h"
#include "hermes_shm/util/compress/zlib.h"
#include "hermes_shm/util/random.h"

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

  // Create a normal distribution
  hshm::NormalDistribution normal;
  normal.Shape(0, 512);
  std::vector<int> data(xfer_count);
  normal.GetInt();
  for (size_t i = 0; i < iter; ++i) {
    data[i] = normal.GetInt();
  }

  // Write compressed data repeatedly
  for (size_t i = 0; i < iter; ++i) {
    if (do_compress) {
      hshm::Zlib compress;
      size_t compressed_size = xfer;
      std::vector<int> compressed(xfer_count);
      compress.Compress(compressed.data(), compressed_size,
                        data.data(), xfer);
      fwrite(compressed.data(), sizeof(char), compressed_size, file);
    } else {
      fwrite(data.data(), sizeof(char), xfer, file);
    }
    fseek(file, 0, SEEK_SET);
  }
  MPI_Finalize();
}