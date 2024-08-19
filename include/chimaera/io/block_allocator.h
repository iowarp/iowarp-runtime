//
// Created by llogan on 7/31/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_IO_BLOCK_ALLOCATOR_H_
#define CHIMAERA_INCLUDE_CHIMAERA_IO_BLOCK_ALLOCATOR_H_

namespace chi {

/** BDEV performance statistics */
struct BdevStats {
  float read_bw_;
  float write_bw_;
  float read_latency_;
  float write_latency_;
  size_t free_;

  template<typename Ar>
  void serialize(Ar &ar) {
    ar(read_bw_, write_bw_, read_latency_, write_latency_, free_);
  }

  friend std::ostream &operator<<(std::ostream &os, const BdevStats &stats) {
    os << hshm::Formatter::format(
        "ReadLat: {}, ReadBW: {}, WriteLat: {}, WriteBw: {}, Free: {}",
        stats.read_latency_, stats.read_bw_, stats.write_latency_,
        stats.write_bw_, stats.free_);
    return os;
  }
};

/** URL for block storage devices */
struct BlockUrl {
  u32 scheme_;
  std::string path_;

  CLS_CONST u32 kFs = 0;
  CLS_CONST u32 kRam = 1;
  CLS_CONST u32 kSpdk = 2;

  void Parse(const std::string &url) {
    // URL Format: <scheme>://<path>
    // Example: spdk://dev/nvme0n1
    // Parse the scheme
    size_t pos = url.find("://");
    if (pos == std::string::npos) {
      path_ = url;
      scheme_ = kFs;
    } else {
      std::string scheme = url.substr(0, pos);
      path_ = url.substr(pos + 3);
      if (scheme == "fs") {
        scheme_ = kFs;
      } else if (scheme == "ram") {
        scheme_ = kRam;
      } else if (scheme == "spdk") {
        scheme_ = kSpdk;
      } else {
        scheme_ = kFs;
      }
    }
  }
};

/** A struct representing a block allocation */
struct Block {
  size_t off_;
  size_t size_;

  template<typename Ar>
  void serialize(Ar &ar) {
    ar(off_, size_);
  }
};

typedef std::list<Block> FREE_LIST;

struct PerCoreFreeList {
  hshm::Mutex lock_;
  std::vector<FREE_LIST> lanes_;

  void resize(int num_lanes) {
    lanes_.resize(num_lanes);
  }
};

struct FreeListMap {
  std::vector<PerCoreFreeList> list_;

  void resize(int num_lanes, int num_free_lists) {
    list_.resize(num_free_lists);
    for (int i = 0; i < num_free_lists; ++i) {
      list_[i].resize(num_lanes);
    }
  }

  std::vector<Block> Aggregate() {
    std::vector<Block> Blocks;
    for (auto &free_list : list_) {
      for (auto &per_core_free_list : free_list.lanes_) {
        for (auto &Block : per_core_free_list) {
          Blocks.push_back(Block);
        }
      }
    }
    return Blocks;
  }
};

struct BlockAllocator {
 public:
  std::atomic<size_t> heap_off_ = 0;
  std::atomic<size_t> free_size_ = 0;
  size_t max_heap_size_;
  FreeListMap free_list_;
  RwLock compact_lock_;

 public:
  void Init(size_t num_lanes, size_t max_heap_size) {
    max_heap_size_ = max_heap_size;
    free_list_.resize(num_lanes, 4);
    free_size_ = max_heap_size;
  }

  Block Allocate(int lane, size_t size) {
    free_size_ -= size;
    if (size <= KILOBYTES(4)) {
      return ListAllocate(KILOBYTES(4), lane, 0);
    } else if (size <= KILOBYTES(16)) {
      return ListAllocate(KILOBYTES(16), lane, 1);
    } else if (size <= KILOBYTES(64)) {
      return ListAllocate(KILOBYTES(64), lane, 2);
    } else {
      return ListAllocate(MEGABYTES(1), lane, 3);
    }
  }

  Block ListAllocate(size_t Block_size, int lane, int free_list_id) {
    FREE_LIST &free_list = free_list_.list_[free_list_id].lanes_[lane];
    if (!free_list.empty()) {
      Block Block = free_list.front();
      free_list.pop_front();
      return Block;
    } else {
      Block block;
      block.off_ = heap_off_.fetch_add(Block_size);
      block.size_ = Block_size;
      return block;
    }
  }

  void Free(int lane, const Block &block) {
    if (block.size_ <= KILOBYTES(4)) {
      free_list_.list_[0].lanes_[lane].push_back(block);
    } else if (block.size_ <= KILOBYTES(16)) {
      free_list_.list_[1].lanes_[lane].push_back(block);
    } else if (block.size_ <= KILOBYTES(64)) {
      free_list_.list_[2].lanes_[lane].push_back(block);
    } else {
      free_list_.list_[3].lanes_[lane].push_back(block);
    }
    free_size_ += block.size_;
  }
};

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_IO_BLOCK_ALLOCATOR_H_
