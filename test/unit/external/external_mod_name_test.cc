/**
 * External application example: Using MOD_NAME ChiMod client libraries
 * 
 * This demonstrates how an external application can link to Chimaera
 * ChiMod libraries using the CMake export system and create a MOD_NAME container.
 * 
 * NOTE: This test is designed to be compiled independently of the main
 * Chimaera build system. It should be compiled manually after installing
 * Chimaera libraries.
 */

#include <iostream>
#include <memory>
#include <chimaera/chimaera.h>
#include <chimaera/MOD_NAME/MOD_NAME_client.h>
#include <chimaera/admin/admin_client.h>

namespace {
constexpr chi::PoolId kExternalTestPoolId = static_cast<chi::PoolId>(7000);
}

int main() {
  std::cout << "=== External MOD_NAME ChiMod Test ===" << std::endl;
  std::cout << "This test demonstrates external linking to Chimaera ChiMod libraries." << std::endl;
  
  try {
    // Step 1: Initialize Chimaera client
    std::cout << "\n1. Initializing Chimaera client..." << std::endl;
    bool client_init_success = chi::CHIMAERA_CLIENT_INIT();
    
    if (!client_init_success) {
      std::cout << "NOTICE: Chimaera client initialization failed." << std::endl;
      std::cout << "This is expected when no runtime is active." << std::endl;
      std::cout << "In a production environment, ensure chimaera_start_runtime is running." << std::endl;
    } else {
      std::cout << "SUCCESS: Chimaera client initialized!" << std::endl;
    }
    
    // Step 2: Create admin client (required for pool management)
    std::cout << "\n2. Creating admin client..." << std::endl;
    chimaera::admin::Client admin_client(chi::kAdminPoolId);
    std::cout << "Admin client created with pool ID: " << chi::kAdminPoolId << std::endl;
    
    // Step 3: Create MOD_NAME client
    std::cout << "\n3. Creating MOD_NAME client..." << std::endl;
    chimaera::MOD_NAME::Client mod_name_client(kExternalTestPoolId);
    std::cout << "MOD_NAME client created with pool ID: " << kExternalTestPoolId << std::endl;
    
    // Step 4: Create MOD_NAME container
    std::cout << "\n4. Creating MOD_NAME container..." << std::endl;
    
    // Use local pool query (recommended default)
    auto pool_query = chi::PoolQuery::Local();
    
    try {
      // This will create the pool if it doesn't exist
      mod_name_client.Create(HSHM_MCTX, pool_query);
      std::cout << "SUCCESS: MOD_NAME container created!" << std::endl;
      
      // Step 5: Demonstrate basic operation
      std::cout << "\n5. Testing MOD_NAME custom operation..." << std::endl;
      std::string input_data = "external_test_data";
      std::string output_data;
      chi::u32 operation_id = 42;
      
      chi::u32 result = mod_name_client.Custom(HSHM_MCTX, pool_query, 
                                              input_data, operation_id, output_data);
      
      std::cout << "Custom operation completed:" << std::endl;
      std::cout << "  Input: " << input_data << std::endl;
      std::cout << "  Output: " << output_data << std::endl;
      std::cout << "  Result code: " << result << std::endl;
      
      std::cout << "\n=== Test completed successfully! ===" << std::endl;
      
    } catch (const std::exception& e) {
      std::cout << "NOTICE: Container creation failed: " << e.what() << std::endl;
      std::cout << "This is expected when no runtime is active." << std::endl;
    }
    
  } catch (const std::exception& e) {
    std::cout << "ERROR: Exception occurred: " << e.what() << std::endl;
    return 1;
  }
  
  std::cout << "\nNOTE: This test demonstrates successful linking to ChiMod libraries." << std::endl;
  std::cout << "For full functionality, run chimaera_start_runtime in another terminal." << std::endl;
  
  return 0;
}