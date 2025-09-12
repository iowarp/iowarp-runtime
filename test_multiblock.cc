#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

// Include Chimaera headers
#include <chimaera/chimaera.h>
#include <chimaera/singletons.h>
#include <chimaera/types.h>
#include <chimaera/pool_query.h>

// Include bdev client and tasks
#include <chimaera/bdev/bdev_client.h>
#include <chimaera/bdev/bdev_tasks.h>

// Include admin client for pool management
#include <chimaera/admin/admin_client.h>
#include <chimaera/admin/admin_tasks.h>

using namespace std::chrono_literals;

const chi::u64 k4KB = 4096;
const chi::u64 k64KB = 65536;
const chi::u64 k256KB = 262144;
const chi::u64 k1MB = 1048576;

int main() {
    std::cout << "=== Multi-Block Allocation Test ===" << std::endl;
    
    // Initialize runtime and client
    std::cout << "1. Initializing Chimaera runtime..." << std::endl;
    if (!chi::CHIMAERA_RUNTIME_INIT()) {
        std::cout << "Failed to initialize runtime" << std::endl;
        return 1;
    }
    
    std::this_thread::sleep_for(500ms);
    
    std::cout << "2. Initializing Chimaera client..." << std::endl;
    if (!chi::CHIMAERA_CLIENT_INIT()) {
        std::cout << "Failed to initialize client" << std::endl;
        return 1;
    }
    
    std::this_thread::sleep_for(200ms);
    
    try {
        // Create admin client first
        std::cout << "3. Creating admin client..." << std::endl;
        chimaera::admin::Client admin_client;
        admin_client.Create(HSHM_MCTX, chi::PoolQuery::Local());
        std::this_thread::sleep_for(100ms);
        
        // Create RAM-based bdev client for testing
        std::cout << "4. Creating bdev client (RAM backend)..." << std::endl;
        chimaera::bdev::Client bdev_client(static_cast<chi::PoolId>(9000));
        
        // Create RAM-based bdev container (10MB)
        const chi::u64 ram_size = 10 * 1024 * 1024;
        bdev_client.Create(HSHM_MCTX, chi::PoolQuery::Local(), 
                          chimaera::bdev::BdevType::kRam, "", ram_size);
        std::this_thread::sleep_for(100ms);
        
        // Test 1: Allocate a size that requires multiple blocks
        std::cout << "\n=== Test 1: Multi-block allocation (1.5MB) ===" << std::endl;
        const chi::u64 test_size_1 = k1MB + k256KB + k64KB + k4KB;  // 1.5MB
        std::cout << "Requesting " << test_size_1 << " bytes (1.5MB)" << std::endl;
        
        chimaera::bdev::Block block1 = bdev_client.Allocate(HSHM_MCTX, test_size_1);
        
        if (block1.size_ >= test_size_1) {
            std::cout << "✓ Single block allocation successful:" << std::endl;
            std::cout << "  Block size: " << block1.size_ << " bytes" << std::endl;
            std::cout << "  Block type: " << block1.block_type_ << std::endl;
            std::cout << "  Block offset: " << block1.offset_ << std::endl;
        } else {
            std::cout << "✗ Allocation failed or insufficient size" << std::endl;
            std::cout << "  Returned size: " << block1.size_ << " bytes" << std::endl;
        }
        
        // Test 2: Multiple sequential allocations
        std::cout << "\n=== Test 2: Sequential allocations ===" << std::endl;
        std::vector<chimaera::bdev::Block> blocks;
        
        // Allocate blocks of different sizes
        std::vector<chi::u64> test_sizes = {k4KB, k64KB, k256KB, k1MB};
        
        for (size_t i = 0; i < test_sizes.size(); ++i) {
            chi::u64 size = test_sizes[i];
            std::cout << "Allocating " << size << " bytes..." << std::endl;
            
            chimaera::bdev::Block block = bdev_client.Allocate(HSHM_MCTX, size);
            
            if (block.size_ >= size) {
                std::cout << "  ✓ Success: offset=" << block.offset_ 
                         << ", size=" << block.size_ 
                         << ", type=" << block.block_type_ << std::endl;
                blocks.push_back(block);
            } else {
                std::cout << "  ✗ Failed: returned size=" << block.size_ << std::endl;
            }
        }
        
        // Test 3: Check for overlapping blocks
        std::cout << "\n=== Test 3: Checking for block overlaps ===" << std::endl;
        bool no_overlaps = true;
        
        for (size_t i = 0; i < blocks.size(); ++i) {
            for (size_t j = i + 1; j < blocks.size(); ++j) {
                chi::u64 end_i = blocks[i].offset_ + blocks[i].size_;
                chi::u64 start_j = blocks[j].offset_;
                chi::u64 end_j = blocks[j].offset_ + blocks[j].size_;
                chi::u64 start_i = blocks[i].offset_;
                
                // Check for overlap
                if (!((end_i <= start_j) || (end_j <= start_i))) {
                    std::cout << "✗ OVERLAP DETECTED between block " << i << " and " << j << std::endl;
                    std::cout << "  Block " << i << ": [" << start_i << " - " << end_i << "]" << std::endl;
                    std::cout << "  Block " << j << ": [" << start_j << " - " << end_j << "]" << std::endl;
                    no_overlaps = false;
                }
            }
        }
        
        if (no_overlaps) {
            std::cout << "✓ No overlaps detected - allocation working correctly" << std::endl;
        }
        
        // Test 4: Test I/O on multi-block allocation
        std::cout << "\n=== Test 4: I/O on allocated block ===" << std::endl;
        if (!blocks.empty()) {
            const chimaera::bdev::Block& test_block = blocks[0];
            
            // Create test data
            std::vector<hshm::u8> write_data(test_block.size_);
            for (size_t i = 0; i < write_data.size(); ++i) {
                write_data[i] = static_cast<hshm::u8>((i + 0xAB) % 256);
            }
            
            // Write data
            std::cout << "Writing " << write_data.size() << " bytes..." << std::endl;
            chi::u64 bytes_written = bdev_client.Write(HSHM_MCTX, test_block, write_data);
            
            if (bytes_written == write_data.size()) {
                std::cout << "✓ Write successful" << std::endl;
                
                // Read data back
                std::cout << "Reading data back..." << std::endl;
                std::vector<hshm::u8> read_data = bdev_client.Read(HSHM_MCTX, test_block);
                
                if (read_data.size() == write_data.size()) {
                    // Verify data integrity
                    bool data_matches = true;
                    for (size_t i = 0; i < write_data.size(); ++i) {
                        if (read_data[i] != write_data[i]) {
                            data_matches = false;
                            std::cout << "✗ Data mismatch at index " << i 
                                     << ": expected " << (int)write_data[i] 
                                     << ", got " << (int)read_data[i] << std::endl;
                            break;
                        }
                    }
                    
                    if (data_matches) {
                        std::cout << "✓ Read successful - data integrity verified" << std::endl;
                    }
                } else {
                    std::cout << "✗ Read size mismatch: expected " << write_data.size() 
                             << ", got " << read_data.size() << std::endl;
                }
            } else {
                std::cout << "✗ Write failed: wrote " << bytes_written 
                         << " bytes, expected " << write_data.size() << std::endl;
            }
        }
        
        // Test 5: Free blocks
        std::cout << "\n=== Test 5: Freeing allocated blocks ===" << std::endl;
        
        // Free the first block
        if (block1.size_ > 0) {
            chi::u32 free_result = bdev_client.Free(HSHM_MCTX, block1);
            if (free_result == 0) {
                std::cout << "✓ Successfully freed multi-block allocation" << std::endl;
            } else {
                std::cout << "✗ Failed to free multi-block allocation: " << free_result << std::endl;
            }
        }
        
        // Free other blocks
        for (size_t i = 0; i < blocks.size(); ++i) {
            chi::u32 free_result = bdev_client.Free(HSHM_MCTX, blocks[i]);
            if (free_result == 0) {
                std::cout << "✓ Successfully freed block " << i << std::endl;
            } else {
                std::cout << "✗ Failed to free block " << i << ": " << free_result << std::endl;
            }
        }
        
        std::cout << "\n=== Test Complete ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
        return 1;
    }
}