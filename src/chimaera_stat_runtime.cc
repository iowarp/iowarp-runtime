#include "chimaera/chimaera_types.h"
#include "chimaera_admin/chimaera_admin.h"

int main() {
  CHIMAERA_CLIENT_INIT();
  while (true) {
    std::vector<chi::WorkerStats> stats = CHI_ADMIN->PollStats(
        HSHM_DEFAULT_MEM_CTX, chi::DomainQuery::GetLocalHash(0));
    for (const auto &stat : stats) {
      std::cout << "\033[33m" << stat << "\033[0m" << std::endl;
    }
    sleep(1);
  }
}