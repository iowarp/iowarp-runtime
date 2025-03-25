#include "compressor/compressor_client.h"

int main() {
  chi::compressor::Client client;
  client.Create(
      HSHM_MCTX,
      chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers, 0),
      chi::DomainQuery::GetGlobalBcast(), "ipc_test");

  size_t data_size = hshm::Unit<size_t>::Megabytes(1);
  hipc::FullPtr<char> orig_data =
      CHI_CLIENT->AllocateBuffer(HSHM_MCTX, data_size);
  //   client.Compress(HSHM_MCTX, chi::DomainQuery::GetLocalHash(0), data,
  //                   data_size);
  return 0;
}