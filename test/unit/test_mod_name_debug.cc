/**
 * Debug test for MOD_NAME container creation
 */

#include <simple_test.h>
#include <chimaera/chimaera.h>
#include <MOD_NAME/MOD_NAME_client.h>
#include <iostream>

namespace {

class ChimaeraTestFixture {
 public:
  ChimaeraTestFixture() {
    chi::CHIMAERA_RUNTIME_INIT();
  }
};

}  // anonymous namespace

TEST_CASE("MOD_NAME Container Creation Debug", "[debug][mod_name]") {
  ChimaeraTestFixture fixture;

  SECTION("Try to create MOD_NAME container") {
    std::cout << "Starting MOD_NAME container creation test..." << std::endl;
    
    // Create MOD_NAME client
    const chi::PoolId mod_name_pool_id = static_cast<chi::PoolId>(4000);
    chimaera::MOD_NAME::Client mod_name_client(mod_name_pool_id);
    std::cout << "MOD_NAME client created with pool ID: " << mod_name_pool_id.ToU64() << std::endl;

    // Try to create the container
    std::cout << "About to call AsyncCreate..." << std::endl;
    auto create_task = mod_name_client.AsyncCreate(HSHM_MCTX, chi::PoolQuery::Local());
    std::cout << "AsyncCreate returned, waiting..." << std::endl;
    
    create_task->Wait();
    std::cout << "Wait completed, result code: " << create_task->result_code_ << std::endl;
    
    REQUIRE(create_task->result_code_ == 0);
    
    // Clean up
    auto* ipc_manager = CHI_IPC;
    ipc_manager->DelTask(create_task);
    
    std::cout << "MOD_NAME container creation test completed successfully" << std::endl;
  }
}

SIMPLE_TEST_MAIN()