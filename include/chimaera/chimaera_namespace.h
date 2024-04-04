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

#ifndef HRUN_INCLUDE_HRUN_HRUN_NAMESPACE_H_
#define HRUN_INCLUDE_HRUN_HRUN_NAMESPACE_H_

#include "chimaera/api/chimaera_client.h"
#include "chimaera/task_registry/task_lib.h"

using chm::TaskMethod;
using chm::BinaryOutputArchive;
using chm::BinaryInputArchive;
using chm::Task;
using chm::TaskPointer;
using chm::MultiQueue;
using chm::PriorityInfo;
using chm::TaskNode;
using chm::DomainId;
using chm::TaskStateId;
using chm::QueueId;
using chm::TaskFlags;
using chm::DataTransfer;
using chm::TaskLib;
using chm::TaskLibClient;
using chm::config::QueueManagerInfo;
using chm::TaskPrio;
using chm::RunContext;

using hshm::RwLock;
using hshm::Mutex;
using hshm::bitfield;
using hshm::bitfield32_t;
typedef hshm::bitfield<uint64_t> bitfield64_t;
using hshm::ScopedRwReadLock;
using hshm::ScopedRwWriteLock;
using hipc::LPointer;

#endif  // HRUN_INCLUDE_HRUN_HRUN_NAMESPACE_H_
