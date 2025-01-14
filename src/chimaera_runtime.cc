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

#include "chimaera/api/chimaera_runtime.h"

#include <hermes_shm/util/singleton.h>

#include "chimaera/module_registry/task.h"

namespace chi {

/** Finalize Hermes explicitly */
void Runtime::Finalize() {}

/** Run the Hermes core Daemon */
void Runtime::RunDaemon() {
  thallium_.RunDaemon();
  HILOG(kInfo, "(node {}) Finishing up last requests", CHI_CLIENT->node_id_);
  CHI_WORK_ORCHESTRATOR->Join();
  HILOG(kInfo, "(node {}) Daemon is exiting", CHI_CLIENT->node_id_);
}

/** Stop the Hermes core Daemon */
void Runtime::StopDaemon() { CHI_WORK_ORCHESTRATOR->FinalizeRuntime(); }

}  // namespace chi