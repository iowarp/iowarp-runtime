// Test file for clangd header resolution
#include <chimaera/chimaera.h>
#include <chimaera/admin/admin_client.h>
#include <chimaera/bdev/bdev_client.h>
#include <chimaera/MOD_NAME/MOD_NAME_client.h>

int main() {
    // Test that basic types are resolved
    chi::PoolId pool_id;
    chi::Task* task;
    
    // Test that ChiMod classes are resolved
    chimaera::admin::Client admin_client;
    chimaera::bdev::Client bdev_client;
    chimaera::MOD_NAME::Client mod_client;
    
    return 0;
}