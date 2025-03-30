/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <malloc.h>
#include <stdlib.h>

// Dynamically checked to see which are the real APIs and which are intercepted
bool malloc_intercepted = true;

#include "chimaera/api/chimaera_client.h"
#include "chimaera_malloc.h"
#include "hermes_shm/memory/memory_manager.h"

using hshm::ipc::Allocator;
using hshm::ipc::Pointer;

namespace chi {
HSHM_DEFINE_GLOBAL_VAR_CC(chi::MallocApi, chiMallocApi);
}  // namespace chi

/** Allocate SIZE bytes of memory. */
void* malloc(size_t size) {
  if (!CHI_MALLOC->is_loaded_) {
    new (CHI_MALLOC) chi::MallocApi();
  }
  if (size < hshm::Unit<size_t>::Megabytes(1) || !CHI_CLIENT->IsInitialized()) {
    return CHI_MALLOC->malloc(size);
  } else {
    return CHI_CLIENT->AllocateBuffer(HSHM_MCTX, size).ptr_;
  }
}

/** Allocate NMEMB elements of SIZE bytes each, all initialized to 0. */
void* calloc(size_t nmemb, size_t size) {
  size_t total_size = nmemb * size;
  void* ptr = malloc(total_size);
  if (ptr) {
    memset(ptr, 0, total_size);
  }
  return ptr;
}

/**
 * Re-allocate the previously allocated block in ptr, making the new
 * block SIZE bytes long.
 * */
void* realloc(void* ptr, size_t size) {
  if (ptr == nullptr) {
    return malloc(size);
  }
  if (!CHI_CLIENT->IsInitialized() ||
      !CHI_CLIENT->data_alloc_->ContainsPtr(ptr)) {
    return CHI_MALLOC->realloc(ptr, size);
  } else {
    hipc::FullPtr<char> p((char*)ptr);
    CHI_CLIENT->data_alloc_->ReallocateLocalPtr(HSHM_MCTX, p, size);
    return p.ptr_;
  }
}

/** Free a block allocated by `malloc', `realloc' or `calloc'. */
void free(void* ptr) {
  if (!CHI_CLIENT->IsInitialized() ||
      !CHI_CLIENT->data_alloc_->ContainsPtr(ptr)) {
    CHI_MALLOC->free(ptr);
  } else {
    hipc::FullPtr<char> p((char*)ptr);
    CHI_CLIENT->FreeBuffer(HSHM_MCTX, p);
  }
}
