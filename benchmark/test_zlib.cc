//
// Created by llogan on 6/25/24.
//
#include <hermes_shm/util/config_parse.h>
#include "mpi.h"
#include "hermes_shm/util/compress/zlib.h"
#include "hermes_shm/util/random.h"

int main(int argc, char *argv[]) {
  MPI_Init(&argc, &argv);
  size_t total_size = hshm::ConfigParse::ParseSize(argv[1]);
  size_t io_size = hshm::ConfigParse::ParseSize(argv[2]);
  char *path = argv[3];
  size_t io_count = io_size / sizeof(int);
  size_t iter = total_size / io_size;

  // Open a file stdio
  FILE *file = fopen(path, "w");
  if (file == NULL) {
    fprintf(stderr, "Failed to open file\n");
    return 1;
  }

  // Create a normal distribution
  hshm::NormalDistribution normal;
  normal.Shape(0, 512);
  std::vector<int> data(io_count);
  normal.GetInt();
  for (size_t i = 0; i < iter; ++i) {
    data[i] = normal.GetInt();
  }

  // Write compressed data repeatedly
  for (size_t i = 0; i < iter; ++i) {
    hshm::Zlib compress;
    size_t compressed_size = io_size;
    std::vector<int> compressed(io_count);
    compress.Compress(compressed.data(), compressed_size,
                      data.data(), io_size);
    fwrite(compressed.data(), sizeof(char), compressed_size, file);
    fseek(file, 0, SEEK_SET);
  }
  MPI_Finalize();
}