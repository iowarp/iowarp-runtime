//
// Created by llogan on 6/25/24.
//
#include <hermes_shm/util/affinity.h>
#include <hermes_shm/util/compress/snappy.h>
#include <hermes_shm/util/random.h>

#include "chimaera/chimaera_types.h"
#include "mpi.h"

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

  // Make this rank affine to CPU rank
  hshm::ProcessAffiner::SetCpuAffinity(getpid(), rank);
  // Get core freqeuency of CPU rank
  int prior_cpu_freq = HSHM_SYSTEM_INFO->GetCpuFreqKhz(rank);
  // Set core frequency of CPU rank
  int min_cpu_freq = HSHM_SYSTEM_INFO->GetCpuMinFreqKhz(rank);
  int max_cpu_freq = HSHM_SYSTEM_INFO->GetCpuMaxFreqKhz(rank);
  HSHM_SYSTEM_INFO->SetCpuFreqKhz(rank, min_cpu_freq);

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
      compress.Compress(compressed.data(), compressed_size, data.data(), xfer);
      // fwrite(compressed.data(), sizeof(char), compressed_size, file);
      write_sz = compressed_size;
    } else {
      fwrite(data.data(), sizeof(char), xfer, file);
      write_sz = xfer;
    }
    fflush(file);
    t.Pause();
    // fseek(file, 0, SEEK_SET);
    //    HILOG(kInfo, "Wrote {} bytes in {} usec (compress={})",
    //          write_sz, t.GetUsec(), do_compress);
  }

  // Get core frequency of CPU rank
  int cur_cpu_freq = HSHM_SYSTEM_INFO->GetCpuFreqKhz(rank);
  HILOG(kInfo, "Rank {} CPU freq: {} -> {}, min={}, max={}", rank,
        prior_cpu_freq, cur_cpu_freq, min_cpu_freq, max_cpu_freq);

  // Set core frequency of CPU rank
  HSHM_SYSTEM_INFO->SetCpuMinFreqKhz(rank, min_cpu_freq);
  HSHM_SYSTEM_INFO->SetCpuMaxFreqKhz(rank, max_cpu_freq);

  MPI_Finalize();
}