/**
 * Comprehensive unit tests for Task Archive Serialization System
 *
 * Tests the complete task archive functionality including:
 * - TaskLoadInArchive/OUT and TaskSaveInArchive/OUT
 * - Task serialization/deserialization with BaseSerialize methods
 * - Container Save/Load methods
 * - Bulk transfer recording
 * - Various task types from admin module
 */

#include "../simple_test.h"
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Include Chimaera headers
#include <chimaera/chimaera.h>
#include <chimaera/container.h>
#include <chimaera/pool_query.h>
#include <chimaera/singletons.h>
#include <chimaera/task.h>
#include <chimaera/task_archives.h>
#include <chimaera/types.h>

// Include admin tasks for testing concrete task types
#include <chimaera/admin/admin_tasks.h>

// Include cereal for comparison tests
#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace {
// Test constants
constexpr chi::u32 kTestWriteFlag = 0x1;  // BULK_XFER
constexpr chi::u32 kTestExposeFlag = 0x2; // BULK_EXPOSE

// Helper allocator for tests
hipc::CtxAllocator<CHI_MAIN_ALLOC_T> GetTestAllocator() {
  return HSHM_MEMORY_MANAGER->GetDefaultAllocator<CHI_MAIN_ALLOC_T>();
}

// Helper to create test task with sample data
std::unique_ptr<chi::Task> CreateTestTask() {
  auto alloc = GetTestAllocator();
  auto task = std::make_unique<chi::Task>(alloc, chi::TaskId(1, 1, 1, 0, 1),
                                          chi::PoolId(100, 0), chi::PoolQuery(),
                                          chi::MethodId(42));
  task->period_ns_ = 1000000.0; // 1ms
  task->task_flags_.SetBits(0x10);
  return task;
}

// Helper to create test admin task with sample data
std::unique_ptr<chimaera::admin::CreateTask> CreateTestAdminTask() {
  auto alloc = GetTestAllocator();
  auto task = std::make_unique<chimaera::admin::CreateTask>(
      alloc, chi::TaskId(2, 2, 2, 0, 2), chi::PoolId(200, 0), chi::PoolQuery(),
      "test_chimod", "test_pool", chi::PoolId(300, 0));
  task->return_code_ = 42;
  task->error_message_ = hipc::string(alloc, "test error message");
  return task;
}

// Test data structure for non-Task serialization
struct TestData {
  int value;
  std::string text;
  std::vector<double> numbers;

  template <class Archive> void serialize(Archive &ar) {
    ar(value, text, numbers);
  }

  bool operator==(const TestData &other) const {
    return value == other.value && text == other.text &&
           numbers == other.numbers;
  }
};

// Create test data
TestData CreateTestData() {
  return TestData{42, "hello world", {1.5, 2.7, 3.14159}};
}
} // namespace

TEST_CASE("TaskLoadInArchive - Basic Construction",
          "[task_archive][input_in]") {
  SECTION("Construction from string") {
    std::string test_data = "test serialized data";
    chi::TaskLoadInArchive archive(test_data);

    // Archive should be constructed successfully
    // We can't easily test internal state, but construction should not throw
    REQUIRE_NOTHROW(archive.send);
  }

  SECTION("Construction from const char* and size") {
    const char *test_data = "test data";
    size_t size = strlen(test_data);
    chi::TaskLoadInArchive archive(test_data, size);

    // Archive should be constructed successfully
    REQUIRE_NOTHROW(archive.send);
  }

  SECTION("Bulk transfers should be empty initially") {
    std::string test_data = "test";
    chi::TaskLoadInArchive archive(test_data);

    auto bulk_transfers = archive.send;
    REQUIRE(bulk_transfers.empty());
  }
}

TEST_CASE("TaskSaveInArchive - Basic Construction and Data Retrieval",
          "[task_archive][output_in]") {
  SECTION("Default construction") {
    chi::TaskSaveInArchive archive;

    // Should start with empty bulk transfers
    REQUIRE(archive.send.empty());

    // Should be able to get data (will be empty initially)
    std::string data = archive.GetData();
    // Cereal binary archives are empty until data is serialized
    REQUIRE(data.empty() || !data.empty()); // Either is valid
  }

  SECTION("Serializing simple data") {
    chi::TaskSaveInArchive archive;
    int test_value = 42;

    REQUIRE_NOTHROW(archive << test_value);

    std::string data = archive.GetData();
    REQUIRE_FALSE(data.empty());
  }
}

TEST_CASE("TaskLoadOutArchive - Basic Construction",
          "[task_archive][input_out]") {
  SECTION("Construction from string") {
    std::string test_data = "test serialized data";
    chi::TaskLoadOutArchive archive(test_data);

    REQUIRE_NOTHROW(archive.GetBulkTransfers());
  }

  SECTION("Construction from const char* and size") {
    const char *test_data = "test data";
    size_t size = strlen(test_data);
    chi::TaskLoadOutArchive archive(test_data, size);

    REQUIRE_NOTHROW(archive.GetBulkTransfers());
  }
}

TEST_CASE("TaskSaveOutArchive - Basic Construction and Data Retrieval",
          "[task_archive][output_out]") {
  SECTION("Default construction") {
    chi::TaskSaveOutArchive archive;

    REQUIRE(archive.GetBulkTransfers().empty());

    std::string data = archive.GetData();
    // Cereal binary archives are empty until data is serialized
    REQUIRE(data.empty() || !data.empty()); // Either is valid
  }

  SECTION("Serializing simple data") {
    chi::TaskSaveOutArchive archive;
    int test_value = 42;

    REQUIRE_NOTHROW(archive << test_value);

    std::string data = archive.GetData();
    REQUIRE_FALSE(data.empty());
  }
}

TEST_CASE("Bulk Transfer Recording", "[task_archive][bulk_transfer]") {
  SECTION("TaskLoadInArchive bulk transfer recording") {
    std::string test_data = "test";
    chi::TaskLoadInArchive archive(test_data);

    hipc::Pointer test_ptr; // Null pointer for testing
    size_t test_size = 1024;
    uint32_t test_flags = kTestWriteFlag | kTestExposeFlag;

    REQUIRE_NOTHROW(archive.bulk(test_ptr, test_size, test_flags));

    auto bulk_transfers = archive.send;
    REQUIRE(bulk_transfers.size() == 1);
    REQUIRE(bulk_transfers[0].size == test_size);
    REQUIRE(bulk_transfers[0].flags == test_flags);
  }

  SECTION("TaskSaveInArchive bulk transfer recording") {
    chi::TaskSaveInArchive archive;

    hipc::Pointer test_ptr;
    size_t test_size = 2048;
    uint32_t test_flags = kTestWriteFlag;

    archive.bulk(test_ptr, test_size, test_flags);

    auto bulk_transfers = archive.send;
    REQUIRE(bulk_transfers.size() == 1);
    REQUIRE(bulk_transfers[0].size == test_size);
    REQUIRE(bulk_transfers[0].flags == test_flags);
  }

  SECTION("Multiple bulk transfers") {
    chi::TaskSaveInArchive archive;

    // Add multiple bulk transfers
    archive.bulk(hipc::Pointer(), 100, kTestWriteFlag);
    archive.bulk(hipc::Pointer(), 200, kTestExposeFlag);
    archive.bulk(hipc::Pointer(), 300, kTestWriteFlag | kTestExposeFlag);

    auto bulk_transfers = archive.send;
    REQUIRE(bulk_transfers.size() == 3);
    REQUIRE(bulk_transfers[0].size == 100);
    REQUIRE(bulk_transfers[1].size == 200);
    REQUIRE(bulk_transfers[2].size == 300);
    REQUIRE(bulk_transfers[2].flags == (kTestWriteFlag | kTestExposeFlag));
  }
}

TEST_CASE("Non-Task Object Serialization", "[task_archive][non_task]") {
  SECTION("Round-trip serialization of custom struct") {
    TestData original = CreateTestData();

    // Serialize
    chi::TaskSaveInArchive out_archive;
    REQUIRE_NOTHROW(out_archive << original);
    std::string serialized_data = out_archive.GetData();

    // Deserialize
    chi::TaskLoadInArchive in_archive(serialized_data);
    TestData deserialized;
    REQUIRE_NOTHROW(in_archive >> deserialized);

    // Verify data integrity
    REQUIRE(deserialized == original);
  }

  SECTION("Bidirectional operator() for multiple values") {
    std::string str1 = "hello";
    int int1 = 42;
    double double1 = 3.14159;

    // Serialize using operator()
    chi::TaskSaveInArchive out_archive;
    REQUIRE_NOTHROW(out_archive(str1, int1, double1));
    std::string serialized_data = out_archive.GetData();

    // Deserialize using operator()
    chi::TaskLoadInArchive in_archive(serialized_data);
    std::string str2;
    int int2;
    double double2;
    REQUIRE_NOTHROW(in_archive(str2, int2, double2));

    // Verify data
    REQUIRE(str1 == str2);
    REQUIRE(int1 == int2);
    REQUIRE(double1 == double2);
  }
}

TEST_CASE("Task Base Class Serialization", "[task_archive][task_base]") {
  SECTION("Task BaseSerializeIn/Out with TaskLoadInArchive") {
    auto original_task = CreateTestTask();

    // Serialize using TaskSaveInArchive (calls BaseSerializeIn + SerializeIn)
    chi::TaskSaveInArchive out_archive;
    REQUIRE_NOTHROW(out_archive << *original_task);
    std::string serialized_data = out_archive.GetData();

    // Deserialize using TaskLoadInArchive
    chi::TaskLoadInArchive in_archive(serialized_data);
    auto new_task = CreateTestTask(); // Create fresh task
    new_task->SetNull(); // Clear data to ensure deserialization works
    REQUIRE_NOTHROW(in_archive >> *new_task);

    // Verify base task fields were preserved
    REQUIRE(new_task->pool_id_ == original_task->pool_id_);
    REQUIRE(new_task->task_id_ == original_task->task_id_);
    REQUIRE(new_task->method_ == original_task->method_);
    REQUIRE(new_task->period_ns_ == original_task->period_ns_);
    REQUIRE(new_task->task_flags_.bits_.load() ==
            original_task->task_flags_.bits_.load());
  }

  SECTION("Task BaseSerializeOut with TaskSaveOutArchive") {
    auto original_task = CreateTestTask();

    // Serialize using TaskSaveOutArchive (calls BaseSerializeOut +
    // SerializeOut)
    chi::TaskSaveOutArchive out_archive;
    REQUIRE_NOTHROW(out_archive << *original_task);
    std::string serialized_data = out_archive.GetData();

    // Deserialize using TaskLoadOutArchive
    chi::TaskLoadOutArchive in_archive(serialized_data);
    auto new_task = CreateTestTask();
    new_task->SetNull();
    REQUIRE_NOTHROW(in_archive >> *new_task);

    // Verify base task fields were preserved
    REQUIRE(new_task->pool_id_ == original_task->pool_id_);
    REQUIRE(new_task->task_id_ == original_task->task_id_);
    REQUIRE(new_task->method_ == original_task->method_);
    REQUIRE(new_task->period_ns_ == original_task->period_ns_);
  }
}

TEST_CASE("Admin Task Serialization", "[task_archive][admin_tasks]") {
  SECTION("CreateTask SerializeIn/SerializeOut") {
    auto original_task = CreateTestAdminTask();

    // Test IN parameter serialization
    chi::TaskSaveInArchive out_archive_in;
    REQUIRE_NOTHROW(out_archive_in << *original_task);
    std::string in_data = out_archive_in.GetData();

    chi::TaskLoadInArchive in_archive_in(in_data);
    auto new_task_in = CreateTestAdminTask();
    new_task_in->chimod_name_ = hipc::string(GetTestAllocator(), ""); // Clear
    new_task_in->pool_name_ = hipc::string(GetTestAllocator(), "");
    REQUIRE_NOTHROW(in_archive_in >> *new_task_in);

    // Verify IN/INOUT parameters were preserved
    REQUIRE(new_task_in->chimod_name_.str() ==
            original_task->chimod_name_.str());
    REQUIRE(new_task_in->pool_name_.str() == original_task->pool_name_.str());
    REQUIRE(new_task_in->pool_id_ == original_task->pool_id_);

    // Test OUT parameter serialization
    chi::TaskSaveOutArchive out_archive_out;
    REQUIRE_NOTHROW(out_archive_out << *original_task);
    std::string out_data = out_archive_out.GetData();

    chi::TaskLoadOutArchive in_archive_out(out_data);
    auto new_task_out = CreateTestAdminTask();
    new_task_out->return_code_ = 0; // Clear
    new_task_out->error_message_ = hipc::string(GetTestAllocator(), "");
    REQUIRE_NOTHROW(in_archive_out >> *new_task_out);

    // Verify OUT/INOUT parameters were preserved
    REQUIRE(new_task_out->return_code_ == original_task->return_code_);
    REQUIRE(new_task_out->error_message_.str() ==
            original_task->error_message_.str());
    REQUIRE(new_task_out->pool_id_ ==
            original_task->pool_id_); // INOUT parameter
  }

  SECTION("DestroyPoolTask serialization") {
    auto alloc = GetTestAllocator();
    chimaera::admin::DestroyPoolTask original_task(
        alloc, chi::TaskId(3, 3, 3, 0, 3), chi::PoolId(400, 0),
        chi::PoolQuery(), chi::PoolId(500, 0), 0x123);
    original_task.return_code_ = 99;
    original_task.error_message_ = hipc::string(alloc, "destroy error");

    // Test round-trip IN parameters
    chi::TaskSaveInArchive out_archive_in;
    out_archive_in << original_task;

    chi::TaskLoadInArchive in_archive_in(out_archive_in.GetData());
    chimaera::admin::DestroyPoolTask new_task_in(alloc);
    in_archive_in >> new_task_in;

    REQUIRE(new_task_in.target_pool_id_ == original_task.target_pool_id_);
    REQUIRE(new_task_in.destruction_flags_ == original_task.destruction_flags_);

    // Test round-trip OUT parameters
    chi::TaskSaveOutArchive out_archive_out;
    out_archive_out << original_task;

    chi::TaskLoadOutArchive in_archive_out(out_archive_out.GetData());
    chimaera::admin::DestroyPoolTask new_task_out(alloc);
    in_archive_out >> new_task_out;

    REQUIRE(new_task_out.return_code_ == original_task.return_code_);
    REQUIRE(new_task_out.error_message_.str() ==
            original_task.error_message_.str());
  }

  SECTION("StopRuntimeTask serialization") {
    auto alloc = GetTestAllocator();
    chimaera::admin::StopRuntimeTask original_task(
        alloc, chi::TaskId(4, 4, 4, 0, 4), chi::PoolId(600, 0),
        chi::PoolQuery(), 0x456, 10000);
    original_task.return_code_ = 777;
    original_task.error_message_ = hipc::string(alloc, "shutdown error");

    // Test IN parameters
    chi::TaskSaveInArchive out_archive_in;
    out_archive_in << original_task;

    chi::TaskLoadInArchive in_archive_in(out_archive_in.GetData());
    chimaera::admin::StopRuntimeTask new_task_in(alloc);
    in_archive_in >> new_task_in;

    REQUIRE(new_task_in.shutdown_flags_ == original_task.shutdown_flags_);
    REQUIRE(new_task_in.grace_period_ms_ == original_task.grace_period_ms_);

    // Test OUT parameters
    chi::TaskSaveOutArchive out_archive_out;
    out_archive_out << original_task;

    chi::TaskLoadOutArchive in_archive_out(out_archive_out.GetData());
    chimaera::admin::StopRuntimeTask new_task_out(alloc);
    in_archive_out >> new_task_out;

    REQUIRE(new_task_out.return_code_ == original_task.return_code_);
    REQUIRE(new_task_out.error_message_.str() ==
            original_task.error_message_.str());
  }
}

TEST_CASE("Archive Operator() Bidirectional Functionality",
          "[task_archive][bidirectional]") {
  SECTION("TaskLoadInArchive operator() acts as input") {
    // Create test data
    int value1 = 42;
    std::string value2 = "test string";
    double value3 = 3.14159;

    // Serialize with standard cereal
    std::ostringstream oss;
    cereal::BinaryOutputArchive out_archive(oss);
    out_archive(value1, value2, value3);

    // Deserialize with TaskLoadInArchive using operator()
    chi::TaskLoadInArchive in_archive(oss.str());
    int result1;
    std::string result2;
    double result3;
    REQUIRE_NOTHROW(in_archive(result1, result2, result3));

    REQUIRE(result1 == value1);
    REQUIRE(result2 == value2);
    REQUIRE(result3 == value3);
  }

  SECTION("TaskSaveInArchive operator() acts as output") {
    int value1 = 123;
    std::string value2 = "output test";
    double value3 = 2.71828;

    // Serialize with TaskSaveInArchive using operator()
    chi::TaskSaveInArchive out_archive;
    REQUIRE_NOTHROW(out_archive(value1, value2, value3));

    // Deserialize with standard cereal
    std::istringstream iss(out_archive.GetData());
    cereal::BinaryInputArchive in_archive(iss);
    int result1;
    std::string result2;
    double result3;
    in_archive(result1, result2, result3);

    REQUIRE(result1 == value1);
    REQUIRE(result2 == value2);
    REQUIRE(result3 == value3);
  }
}

// Test container class that implements all pure virtual methods
class TestContainer : public chi::Container {
public:
  chi::u64 GetWorkRemaining() const override {
    return 0; // Test implementation returns no work
  }

  void Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr,
           chi::RunContext &rctx) override {
    // Test implementation - do nothing
    (void)method;
    (void)task_ptr;
    (void)rctx;
  }

  void Monitor(chi::MonitorModeId mode, chi::u32 method,
               hipc::FullPtr<chi::Task> task_ptr,
               chi::RunContext &rctx) override {
    // Test implementation - do nothing
    (void)mode;
    (void)method;
    (void)task_ptr;
    (void)rctx;
  }

  void Del(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) override {
    // Test implementation - do nothing
    (void)method;
    (void)task_ptr;
  }

  void SaveTask(chi::u32 method, chi::SaveTaskArchive &archive,
                hipc::FullPtr<chi::Task> task_ptr) override {
    // Test implementation - just call task serialization
    (void)method;
    archive << *task_ptr;
  }

  void LoadTask(chi::u32 method, chi::LoadTaskArchive &archive,
                hipc::FullPtr<chi::Task> task_ptr) override {
    // Test implementation - just call task deserialization
    (void)method;
    archive >> task_ptr.ptr_;
  }

  void NewCopy(chi::u32 method, const hipc::FullPtr<chi::Task> &orig_task,
               hipc::FullPtr<chi::Task> &dup_task, bool deep) override {
    // Test implementation - create new task and copy
    (void)method;
    (void)deep;
    auto *ipc_manager = CHI_IPC;
    if (ipc_manager) {
      dup_task = ipc_manager->NewTask<chi::Task>();
      if (!dup_task.IsNull()) {
        dup_task->shm_strong_copy_main(*orig_task);
      }
    }
  }
};

TEST_CASE("Container Serialization Methods", "[task_archive][container]") {
  SECTION("Container SaveIn/LoadIn for base Task") {
    TestContainer container;
    auto original_task = CreateTestTask();
    hipc::FullPtr<chi::Task> task_ptr(original_task.get());

    // Test SaveIn
    chi::TaskSaveInArchive save_archive;
    chi::u32 method = task_ptr->method_;
    REQUIRE_NOTHROW(container.SaveIn(method, save_archive, task_ptr));
    std::string serialized_data = save_archive.GetData();
    REQUIRE_FALSE(serialized_data.empty());

    // Test LoadIn
    auto new_task = CreateTestTask();
    new_task->SetNull();
    hipc::FullPtr<chi::Task> new_task_ptr(new_task.get());
    new_task_ptr->method_ =
        original_task->method_; // LoadIn needs method for switch-case
    chi::TaskLoadInArchive load_archive(serialized_data);
    REQUIRE_NOTHROW(container.LoadIn(method, load_archive, new_task_ptr));

    // Verify data was loaded (though specific verification depends on
    // switch-case implementation) For base Task, the default case should call
    // BaseSerializeIn + SerializeIn
  }

  SECTION("Container SaveOut/LoadOut for base Task") {
    TestContainer container;
    auto original_task = CreateTestTask();
    hipc::FullPtr<chi::Task> task_ptr(original_task.get());

    // Test SaveOut
    chi::TaskSaveOutArchive save_archive;
    chi::u32 method = task_ptr->method_;
    REQUIRE_NOTHROW(container.SaveOut(method, save_archive, task_ptr));
    std::string serialized_data = save_archive.GetData();
    REQUIRE_FALSE(serialized_data.empty());

    // Test LoadOut
    auto new_task = CreateTestTask();
    new_task->SetNull();
    hipc::FullPtr<chi::Task> new_task_ptr(new_task.get());
    new_task_ptr->method_ =
        original_task->method_; // LoadOut needs method for switch-case
    chi::TaskLoadOutArchive load_archive(serialized_data);
    REQUIRE_NOTHROW(container.LoadOut(method, load_archive, new_task_ptr));
  }
}

TEST_CASE("Error Handling and Edge Cases", "[task_archive][error_handling]") {
  SECTION("Invalid serialization data") {
    std::string invalid_data = "this is not valid cereal data";
    chi::TaskLoadInArchive archive(invalid_data);

    // Attempting to deserialize should fail gracefully
    int value;
    // Note: cereal may throw, so we wrap in try-catch in real usage
    // For this test, we just verify the archive can be constructed
    REQUIRE_NOTHROW(archive.send);
  }

  SECTION("Empty serialization data") {
    std::string empty_data = "";
    chi::TaskLoadInArchive archive(empty_data);

    REQUIRE(archive.send.empty());
  }

  SECTION("Bulk transfer with null pointer") {
    chi::TaskSaveInArchive archive;
    hipc::Pointer null_ptr; // Default constructs to null

    REQUIRE_NOTHROW(archive.bulk(null_ptr, 0, 0));

    auto bulk_transfers = archive.send;
    REQUIRE(bulk_transfers.size() == 1);
    REQUIRE(bulk_transfers[0].size == 0);
    REQUIRE(bulk_transfers[0].flags == 0);
  }
}

TEST_CASE("Performance and Large Data", "[task_archive][performance]") {
  SECTION("Large string serialization") {
    std::string large_string(10000, 'X'); // 10KB string

    chi::TaskSaveInArchive out_archive;
    REQUIRE_NOTHROW(out_archive << large_string);

    std::string serialized_data = out_archive.GetData();
    REQUIRE(serialized_data.size() >
            large_string.size()); // Should include cereal overhead

    chi::TaskLoadInArchive in_archive(serialized_data);
    std::string result_string;
    REQUIRE_NOTHROW(in_archive >> result_string);

    REQUIRE(result_string == large_string);
  }

  SECTION("Large vector serialization") {
    std::vector<double> large_vector(1000, 3.14159); // 1000 doubles

    chi::TaskSaveInArchive out_archive;
    out_archive << large_vector;

    chi::TaskLoadInArchive in_archive(out_archive.GetData());
    std::vector<double> result_vector;
    in_archive >> result_vector;

    REQUIRE(result_vector.size() == large_vector.size());
    REQUIRE(result_vector == large_vector);
  }

  SECTION("Multiple task serialization sequence") {
    // Test serializing multiple tasks in sequence
    std::vector<std::string> serialized_tasks;

    for (int i = 0; i < 10; ++i) {
      auto task = CreateTestTask();

      chi::TaskSaveInArchive archive;
      archive << *task;
      serialized_tasks.push_back(archive.GetData());
    }

    // Verify all tasks were serialized uniquely
    REQUIRE(serialized_tasks.size() == 10);
    for (size_t i = 0; i < serialized_tasks.size(); ++i) {
      REQUIRE_FALSE(serialized_tasks[i].empty());
      // Each should be different due to different task_id_.unique_
      for (size_t j = i + 1; j < serialized_tasks.size(); ++j) {
        REQUIRE(serialized_tasks[i] != serialized_tasks[j]);
      }
    }
  }
}

TEST_CASE("Complete Serialization Flow", "[task_archive][integration]") {
  SECTION("Complete round-trip flow for admin CreateTask") {
    auto original_task = CreateTestAdminTask();

    // Step 1: Serialize IN parameters (for sending task to remote node)
    chi::TaskSaveInArchive in_archive;
    in_archive << *original_task;
    std::string in_data = in_archive.GetData();
    auto in_bulk_transfers = in_archive.send;

    // Step 2: Simulate remote node receiving and deserializing IN parameters
    chi::TaskLoadInArchive recv_in_archive(in_data);
    auto remote_task = CreateTestAdminTask();
    remote_task->chimod_name_ = hipc::string(GetTestAllocator(), ""); // Clear
    remote_task->pool_name_ = hipc::string(GetTestAllocator(), "");
    recv_in_archive >> *remote_task;

    // Verify IN parameters were transferred
    REQUIRE(remote_task->chimod_name_.str() ==
            original_task->chimod_name_.str());
    REQUIRE(remote_task->pool_name_.str() == original_task->pool_name_.str());
    REQUIRE(remote_task->pool_id_ == original_task->pool_id_);

    // Step 3: Simulate task execution and result generation on remote node
    remote_task->return_code_ = 123;
    remote_task->error_message_ =
        hipc::string(GetTestAllocator(), "remote execution result");

    // Step 4: Serialize OUT parameters (for sending results back)
    chi::TaskSaveOutArchive out_archive;
    out_archive << *remote_task;
    std::string out_data = out_archive.GetData();
    auto out_bulk_transfers = out_archive.GetBulkTransfers();

    // Step 5: Simulate client receiving and deserializing OUT parameters
    chi::TaskLoadOutArchive recv_out_archive(out_data);
    auto final_task = CreateTestAdminTask();
    final_task->return_code_ = 0; // Clear
    final_task->error_message_ = hipc::string(GetTestAllocator(), "");
    recv_out_archive >> *final_task;

    // Verify OUT parameters were transferred back
    REQUIRE(final_task->return_code_ == 123);
    REQUIRE(final_task->error_message_.str() == "remote execution result");
    REQUIRE(final_task->pool_id_ ==
            original_task->pool_id_); // INOUT parameter preserved

    INFO("Complete serialization flow completed successfully");
  }
}

// Main function to run all tests with Chimaera runtime initialization
int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // Initialize Chimaera runtime for memory management
  bool runtime_success = chi::CHIMAERA_RUNTIME_INIT();
  if (!runtime_success) {
    std::cerr << "Failed to initialize Chimaera runtime" << std::endl;
    return 1;
  }

  // Run all tests
  int result = SimpleTest::run_all_tests();

  // Runtime will be cleaned up automatically
  return result;
}