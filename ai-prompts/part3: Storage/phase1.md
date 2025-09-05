@CLAUDE.md Create a chimod called bdev, which stands for block device. Use the same namespace as MOD_NAME. Make sure to read @doc/MODULE_DEVELOPMENT_GUIDE.md and to use chi_refresh_repo.py when building the module. 

## CreateTask

The parameters for the CreateTask will contain a chi::string inidicating the path to a file to open. 

In the Create function, it will conduct a small benchmark to assess the performance of the device. These performance counters will be stored internally.

## AllocateTask

The task takes as input the amount of data to allocate, which is a u64.

In the runtime, this will implement a simple data allocator, similar to a memory allocator. For now, assume there are 4 different block sizes: 4KB, 64KB, 256KB, 1MB. 

When the AllocateTask comes in, map the size to the next largest size of data. Check the free list for the size type. If there is a free block, then use that. Otherwise, we will increment a heap offset and then allocate a new block off the heap. If there is no space left in the heap, then we should return an error. Do not use strings for the errors, use only numbers.

This task should also maintain the remaining size of data. This should be a simple atomic counter. Allocation decreases the counter.

## FreeTask

Takes as input a block to free. No need for complex free detection or corruption algorithms. 

In the runtime, this will add the block to the most appropriate free list and then increase the available remaining space.

## WriteTask and ReadTask

These tasks are similar. They take as input a Block and then read or write to the file asynchronously.

Bdev uses libaio to read and write data. Use direct I/O if libaio supports it. The data should always be aligned to 4KB offsets in the file, which I believe is the requirement for direct I/O. 

## StatTask

This task takes no inputs. As output it will return the performance and remaining size. 
