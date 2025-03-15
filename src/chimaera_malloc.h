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

#ifndef HSHM_SRC_MEMORY_MEMORY_INTERCEPT_H_
#define HSHM_SRC_MEMORY_MEMORY_INTERCEPT_H_

#include <malloc.h>
#include <stdlib.h>

#include "hermes_shm/util/real_api.h"

extern "C" {
typedef void* (*malloc_t)(size_t);
typedef void* (*calloc_t)(size_t, size_t);
typedef void* (*realloc_t)(void*, size_t);
typedef void (*free_t)(void*);
typedef void* (*memalign_t)(size_t, size_t);
}

namespace chi {

template <typename MallocT>
using PreloadProgress = hshm::PreloadProgress<MallocT>;
using hshm::RealApi;

/** Pointers to the real posix API */
class MallocApi : public RealApi {
 public:
  bool is_loaded_ = false;
  /** The real malloc API methods */
  malloc_t malloc = nullptr;
  calloc_t calloc = nullptr;
  realloc_t realloc = nullptr;
  free_t free = nullptr;

 public:
  MallocApi() : RealApi("malloc", "malloc_intercepted") {
    malloc = (malloc_t)dlsym(real_lib_, "malloc");
    REQUIRE_API(malloc)
    calloc = (calloc_t)dlsym(real_lib_, "calloc");
    REQUIRE_API(calloc)
    realloc = (realloc_t)dlsym(real_lib_, "realloc");
    REQUIRE_API(realloc)
    free = (free_t)dlsym(real_lib_, "free");
    REQUIRE_API(free)
  }
};

}  // namespace chi

// Singleton macros
#include "hermes_shm/util/singleton.h"

#define CHI_MALLOC hshm::Singleton<::chi::MallocApi>::GetInstance()
#define CHI_MALLOC_T chi::MallocApi*

#endif  // HSHM_SRC_MEMORY_MEMORY_INTERCEPT_H_
