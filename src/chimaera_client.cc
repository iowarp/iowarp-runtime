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

#include "hermes_shm/util/singleton.h"
#include "chimaera/api/chimaera_client.h"
#include "chimaera/network/rpc_thallium.h"

namespace chi {

/** Segment transfer */
void SegmentedTransfer::AllocateSegmentsServer() {
  for (DataTransfer &xfer : bulk_) {
    LPointer<char> data = CHI_CLIENT->AllocateBufferServer<TASK_YIELD_ABT>(
        xfer.data_size_);
    xfer.data_ = data.ptr_;
  }
}

}  // namespace chi

/** Runtime singleton */
DEFINE_SINGLETON_CC(chi::Client)