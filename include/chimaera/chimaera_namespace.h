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

#ifndef CHI_INCLUDE_CHI_CHI_NAMESPACE_H_
#define CHI_INCLUDE_CHI_CHI_NAMESPACE_H_

/**
 * This header is used to add chimaera_codegen types to the namespace
 * of the user's ChiMod -- with the exception of the Chimaera
 * Admin, which is already in the chimaera_codegen namespace.
 * */

#include "chimaera/api/chimaera_client.h"
#include "chimaera/module_registry/module.h"
#include "chimaera/queue_manager/queue_manager.h"
#include "chimaera_admin/chimaera_admin_client.h"

#define CHI_NAMESPACE_INIT                       \
  using chi::CreateContext;                      \
  using chi::TaskMethod;                         \
  using chi::BinaryOutputArchive;                \
  using chi::BinaryInputArchive;                 \
  using chi::Task;                               \
  using chi::TaskPointer;                        \
  using chi::ingress::MultiQueue;                \
  using chi::ingress::PriorityInfo;              \
  using chi::TaskNode;                           \
  using chi::DomainQuery;                        \
  using chi::PoolId;                             \
  using chi::QueueId;                            \
  using chi::TaskFlags;                          \
  using chi::DataTransfer;                       \
  using chi::SegmentedTransfer;                  \
  using chi::Module;                             \
  using chi::ModuleClient;                       \
  using chi::config::QueueManagerInfo;           \
  using chi::TaskPrio;                           \
  using chi::TaskPrioOpt;                        \
  using chi::RunContext;                         \
  using chi::MethodId;                           \
  using chi::MonitorModeId;                      \
  using chi::MonitorMode;                        \
  using chi::Lane;                               \
  using chi::LaneGroupId;                        \
  using chi::LaneId;                             \
  using chi::NodeId;                             \
  using hshm::RwLock;                            \
  using hshm::Mutex;                             \
  using hshm::bitfield;                          \
  using hshm::bitfield8_t;                       \
  using hshm::bitfield16_t;                      \
  using hshm::ibitfield;                         \
  using hshm::ibitfield;                         \
  using hshm::abitfield8_t;                      \
  using hshm::abitfield16_t;                     \
  using hshm::aibitfield;                        \
  using hshm::aibitfield;                        \
  typedef hshm::bitfield<uint64_t> bitfield64_t; \
  using hipc::FullPtr;                           \
  using hshm::ScopedRwReadLock;                  \
  using hshm::ScopedRwWriteLock;

#endif  // CHI_INCLUDE_CHI_CHI_NAMESPACE_H_
