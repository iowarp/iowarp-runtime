#include <hermes_shm/util/affinity.h>
#include <hermes_shm/util/timer.h>
#include <mpi.h>

#include <cstddef>

#include "basic_test.h"
#include "bdev/bdev.h"
#include "chimaera/api/chimaera_client.h"
#include "chimaera_admin/chimaera_admin.h"
#include "omp.h"
#include "small_message/small_message.h"

CHI_NAMESPACE_INIT

TEST_CASE("TestMalloc") {
  CHIMAERA_CLIENT_INIT();
  malloc(10);
  malloc(hshm::Unit<size_t>::Megabytes(1));
}
