#ifndef ADMIN_AUTOGEN_METHODS_H_
#define ADMIN_AUTOGEN_METHODS_H_

#include <chimaera/chimaera.h>

/**
 * Auto-generated method definitions for Admin
 * Critical ChiMod for managing pools and runtime lifecycle
 */

namespace chimaera::admin {

namespace Method {
// Inherited methods
GLOBAL_CONST chi::u32 kCreate = 0;
GLOBAL_CONST chi::u32 kDestroy = 1;
GLOBAL_CONST chi::u32 kNodeFailure = 2;
GLOBAL_CONST chi::u32 kRecover = 3;
GLOBAL_CONST chi::u32 kMigrate = 4;
GLOBAL_CONST chi::u32 kUpgrade = 5;

// Admin-specific methods
GLOBAL_CONST chi::u32 kGetOrCreatePool = 10;
GLOBAL_CONST chi::u32 kDestroyPool = 11;
GLOBAL_CONST chi::u32 kStopRuntime = 12;
GLOBAL_CONST chi::u32 kFlush = 13;

// Distributed task scheduling methods
GLOBAL_CONST chi::u32 kClientSendTaskIn = 14;
GLOBAL_CONST chi::u32 kServerRecvTaskIn = 15;
GLOBAL_CONST chi::u32 kServerSendTaskOut = 16;
GLOBAL_CONST chi::u32 kClientRecvTaskOut = 17;
}  // namespace Method

}  // namespace chimaera::admin

#endif  // ADMIN_AUTOGEN_METHODS_H_