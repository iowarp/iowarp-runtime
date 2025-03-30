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

#ifndef CHI_INCLUDE_CHI_NETWORK_SERIALIZE_H_
#define CHI_INCLUDE_CHI_NETWORK_SERIALIZE_H_

#include "chimaera/api/chimaera_client.h"
#include "serialize_defn.h"

namespace chi {

/** Segment transfer */
#ifdef CHIMAERA_RUNTIME
void SegmentedTransfer::AllocateBulksServer() {
  for (DataTransfer &xfer : bulk_) {
    FullPtr<char> data =
        CHI_CLIENT->AllocateBufferRemote(HSHM_MCTX, xfer.data_size_);
    xfer.data_ = data.ptr_;
  }
}

void SegmentedTransfer::FreeBulksServer() {
  for (DataTransfer &xfer : bulk_) {
    FullPtr<char> data((char *)xfer.data_);
    CHI_CLIENT->FreeBuffer(HSHM_MCTX, data.shm_);
  }
}
#endif

}  // namespace chi

#endif  // CHI_INCLUDE_CHI_NETWORK_SERIALIZE_H_
