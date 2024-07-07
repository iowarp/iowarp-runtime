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

#ifndef HRUN_INCLUDE_HRUN_NETWORK_SERIALIZE_H_
#define HRUN_INCLUDE_HRUN_NETWORK_SERIALIZE_H_

#include "serialize_defn.h"
#include "chimaera/api/chimaera_client_defn.h"

namespace chi {

/** Segment transfer */
#ifdef CHIMAERA_RUNTIME
void SegmentedTransfer::AllocateSegmentsServer() {
  for (DataTransfer &xfer : bulk_) {
    LPointer<char> data = CHI_CLIENT->AllocateBufferRemote(
        xfer.data_size_);
    xfer.data_ = data.ptr_;
  }
}
#endif

}  // namespace chi

#endif  // HRUN_INCLUDE_HRUN_NETWORK_SERIALIZE_H_
