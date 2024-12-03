//
// Created by llogan on 7/31/24.
//

#ifndef CHIMAERA_INCLUDE_CHIMAERA_IO_BLOCK_ALLOCATOR_H_
#define CHIMAERA_INCLUDE_CHIMAERA_IO_BLOCK_ALLOCATOR_H_

namespace chi {

/** The number of blocks in slab to allocate */
struct SlabCount {
  size_t count_;
  size_t slab_size_;

  SlabCount() : count_(0), slab_size_(0) {}
};

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
  std::vector<size_t> slab_sizes_;
  std::atomic<size_t> heap_off_ = 0;
  std::atomic<size_t> free_size_ = 0;
  size_t max_heap_size_;
  FreeListMap free_list_;
  RwLock compact_lock_;

 public:
  void Init(size_t num_lanes, size_t max_heap_size) {
    max_heap_size_ = max_heap_size;
    free_size_ = max_heap_size;
    // TODO(llogan): Don't hardcode slab sizes
    slab_sizes_.emplace_back(KILOBYTES(4));
    slab_sizes_.emplace_back(KILOBYTES(16));
    slab_sizes_.emplace_back(KILOBYTES(64));
    slab_sizes_.emplace_back(MEGABYTES(1));
    free_list_.resize(num_lanes, slab_sizes_.size());
  }

  void Allocate(int lane, size_t size,
                chi::ipc::vector<Block> &buffers,
                size_t &total_size) {
    u32 buffer_count = 0;
    std::vector<SlabCount> coins = CoinSelect(lane, size, buffer_count);
    buffers.reserve(buffer_count);
    total_size = 0;
    int slab_idx = 0;
    for (auto &coin : coins) {
      AllocateSlabs(coin.slab_size_,
                    slab_idx,
                    coin.count_,
                    buffers,
                    total_size);
      ++slab_idx;
    }
  }

  void Free(int lane, const Block &block) {
    int free_list_id = 0;
    for (size_t slab_size : slab_sizes_) {
      if (block.size_ <= slab_size) {
        free_list_.list_[free_list_id].lanes_[lane].push_back(block);
        free_size_ += block.size_;
        return;
      }
      ++free_list_id;
    }
  }

 private:
  /** Determine how many of each slab size to allocate */
  std::vector<SlabCount> CoinSelect(int lane, size_t size, u32 &buffer_count) {
    std::vector<SlabCount> coins(slab_sizes_.size());
    size_t rem_size = size;

    while (rem_size) {
      // Find the slab size nearest to the rem_size
      size_t slab_id = 0, slab_size = 0;
      for (auto &slab : free_list_.list_[slab_id].lanes_[lane]) {
        if (slab_sizes_[slab_id] >= rem_size) {
          break;
        }
        ++slab_id;
      }
      if (slab_id == slab_sizes_.size()) { slab_id -= 1; }
      slab_size = slab_sizes_[slab_id];

      // Divide rem_size into slabs
      if (rem_size > slab_size) {
        coins[slab_id].count_ += rem_size / slab_size;
        coins[slab_id].slab_size_ = slab_size;
        rem_size %= slab_size;
      } else {
        coins[slab_id].count_ += 1;
        coins[slab_id].slab_size_ = slab_size;
        rem_size = 0;
      }
      buffer_count += coins[slab_id].count_;
    }

    return coins;
  }

  /** Allocate slabs of a certain size */
  void AllocateSlabs(size_t slab_size,
                     int slab_idx, size_t count,
                     chi::ipc::vector<Block> &buffers,
                     size_t &total_size) {
    for (size_t i = 0; i < count; ++i) {
      Block block = ListAllocate(slab_size, 0, slab_idx);
      buffers.emplace_back(block);
      total_size += slab_size;
    }
  }

  Block ListAllocate(size_t slab_size, int lane, int free_list_id) {
    FREE_LIST &free_list = free_list_.list_[free_list_id].lanes_[lane];
    if (!free_list.empty()) {
      Block Block = free_list.front();
      free_list.pop_front();
      return Block;
    } else {
      Block block;
      block.off_ = heap_off_.fetch_add(slab_size);
      if (block.off_ > max_heap_size_) {
        block.off_ = 0;
        block.size_ = 0;
        return block;
      }
      block.size_ = slab_size;
      return block;
    }
  }
};

}  // namespace chi

#endif //CHIMAERA_INCLUDE_CHIMAERA_IO_BLOCK_ALLOCATOR_H_
