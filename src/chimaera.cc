/**
 * Main Chimaera initialization and global functions
 */

#include "chimaera/chimaera.h"

namespace chi {

bool CHIMAERA_CLIENT_INIT() {
  return CHI_CHIMAERA_MANAGER->ClientInit();
}

bool CHIMAERA_RUNTIME_INIT() {
  return CHI_CHIMAERA_MANAGER->ServerInit();
}



}  // namespace chi