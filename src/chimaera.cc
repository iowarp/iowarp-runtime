/**
 * Main Chimaera initialization and global functions
 */

#include "chimaera/chimaera.h"

namespace chi {

bool CHIMAERA_CLIENT_INIT() {
  return CHI_CHIMAERA->ClientInit();
}

bool CHIMAERA_RUNTIME_INIT() {
  return CHI_CHIMAERA->ServerInit();
}

void CHIMAERA_FINALIZE() {
  CHI_CHIMAERA->Finalize();
}

}  // namespace chi