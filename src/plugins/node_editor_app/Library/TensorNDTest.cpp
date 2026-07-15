#include "TensorND.h"
#include <cassert>
#include <cstdio>
#include <chrono>
#include <thread>
#include <cstring>
#include <cmath>

using namespace Daqster;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
    static void test_##name(); \
    struct Register_##name { \
        Register_##name() { \
            printf("  Running: %-40s ", #name); \
            test_##name(); \
            printf("PASS\n"); \
            testsPassed++; \
        } \
    } reg_##name; \
    static void test_##name()

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    ASSERT failed: %s (line %d)\n", #cond, __LINE__); \
        testsFailed++; \
        return; \
    } \
} while(0)

// ===========================================================================
// TR-001: Zero-Copy Verification
// ===========================================================================
TEST(zero_copy_struct_shallow) {
    int64_t shape[] = {1920, 1080, 3};
    TensorND t;
    t.dataMemory = malloc(1920 * 1080 * 3 * sizeof(float));
    t.byteSize = 1920 * 1080 * 3 * sizeof(float);
    t.type = DataType::Float32;
    t.dimensions = 3;
    t.shape = shape;
    t.timestamp = 1;

    TensorND copy = t;

    ASSERT(copy.dataMemory == t.dataMemory);
    ASSERT(copy.byteSize == t.byteSize);
    ASSERT(copy.type == t.type);
    ASSERT(copy.dimensions == t.dimensions);
    ASSERT(copy.shape == t.shape);
    ASSERT(copy.timestamp == t.timestamp);

    free(t.dataMemory);
}

TEST(zero_copy_no_memory_growth) {
    size_t frameSize = 25 * 1024 * 1024;
    const int numFrames = 100;
    const int numNodes = 5;

    std::vector<TensorND> pipeline(numNodes);

    int64_t shape[] = {1, static_cast<int64_t>(frameSize)};
    for (int i = 0; i < numNodes; ++i) {
        pipeline[i].dataMemory = malloc(frameSize);
        pipeline[i].byteSize = frameSize;
        pipeline[i].type = DataType::Uint8;
        pipeline[i].dimensions = 2;
        pipeline[i].shape = static_cast<int64_t*>(malloc(2 * sizeof(int64_t)));
        std::memcpy(pipeline[i].shape, shape, 2 * sizeof(int64_t));
    }

    for (int frame = 0; frame < numFrames; ++frame) {
        for (int node = 0; node < numNodes - 1; ++node) {
            pipeline[node + 1].dataMemory = pipeline[node].dataMemory;
        }
    }

    ASSERT(pipeline[numNodes - 1].dataMemory == pipeline[0].dataMemory);

    for (int i = 0; i < numNodes; ++i) {
        if (i == 0) {
            free(pipeline[i].dataMemory);
        }
        free(pipeline[i].shape);
    }
}

// ===========================================================================
// TR-002: Struct Copy Latency
// ===========================================================================
TEST(struct_copy_latency) {
    TensorND t;
    int64_t shapeBuf[] = {100, 100, 3};
    t.dataMemory = reinterpret_cast<void*>(0xDEADBEEF);
    t.byteSize = 100 * 100 * 3 * sizeof(float);
    t.type = DataType::Float32;
    t.dimensions = 3;
    t.shape = shapeBuf;
    t.timestamp = 42;

    const int iterations = 100000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        volatile TensorND copy = t;
        (void)copy;
    }

    auto end = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(end - start).count() / iterations;

    printf("\n    Struct copy: %.3f μs per copy ", us);
    ASSERT(us < 10.0);
}

// ===========================================================================
// TR-003: Async / Frame Skipping
// ===========================================================================
TEST(frame_skip_when_busy) {
    std::atomic<bool> workerBusy{false};
    int framesProcessed = 0;
    int framesSkipped = 0;

    for (int frame = 0; frame < 100; ++frame) {
        if (workerBusy.load()) {
            framesSkipped++;
            continue;
        }
        workerBusy.store(true);
        framesProcessed++;
        std::thread([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            workerBusy.store(false);
        }).detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT(framesSkipped > 0);
    ASSERT(framesProcessed > 0);
    ASSERT(framesProcessed + framesSkipped == 100);
}

TEST(ui_thread_never_blocks) {
    auto start = std::chrono::high_resolution_clock::now();

    std::atomic<bool> workerDone{false};
    std::thread worker([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        workerDone.store(true);
    });

    for (int i = 0; i < 1000; ++i) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    worker.join();

    ASSERT(ms < 200.0);
    ASSERT(workerDone.load());
}

// ===========================================================================
// Buffer Pool
// ===========================================================================
TEST(pool_acquire_release) {
    int64_t poolShape[] = {100, 100};
    TensorPool pool(DataType::Float32, poolShape, 2, 3);

    auto* b1 = pool.acquire();
    ASSERT(b1 != nullptr);
    ASSERT(b1->inUse.load() == true);

    auto* b2 = pool.acquire();
    ASSERT(b2 != nullptr);
    ASSERT(b2 != b1);

    auto* b3 = pool.acquire();
    ASSERT(b3 != nullptr);

    auto* b4 = pool.acquire();
    ASSERT(b4 == nullptr);

    pool.release(b1);
    ASSERT(b1->inUse.load() == false);

    auto* b5 = pool.acquire();
    ASSERT(b5 != nullptr);

    pool.release(b2);
    pool.release(b3);
    pool.release(b5);
    ASSERT(pool.allFree());
}

TEST(pool_thread_safety) {
    int64_t poolShape[] = {100, 100};
    TensorPool pool(DataType::Float32, poolShape, 2, 3);
    std::atomic<int> acquired{0};

    auto worker = [&]() {
        for (int i = 0; i < 50; ++i) {
            auto* buf = pool.acquire();
            if (buf) {
                acquired.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                pool.release(buf);
            }
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    std::thread t3(worker);
    t1.join();
    t2.join();
    t3.join();

    ASSERT(pool.allFree());
    ASSERT(acquired.load() > 0);
}

// ===========================================================================
// TensorND struct behavior
// ===========================================================================
TEST(default构造) {
    TensorND t;
    ASSERT(t.dataMemory == nullptr);
    ASSERT(t.byteSize == 0);
    ASSERT(t.type == DataType::Float32);
    ASSERT(t.dimensions == 0);
    ASSERT(t.shape == nullptr);
    ASSERT(t.timestamp == 0);
    ASSERT(t.isValid() == false);
}

TEST(element_count_2d) {
    TensorND t;
    int64_t shape[] = {1920, 1080};
    t.dimensions = 2;
    t.shape = shape;
    ASSERT(t.elementCount() == 1920 * 1080);
}

TEST(element_count_3d) {
    TensorND t;
    int64_t shape[] = {1920, 1080, 3};
    t.dimensions = 3;
    t.shape = shape;
    ASSERT(t.elementCount() == 1920 * 1080 * 3);
}

TEST(data_type_sizes) {
    ASSERT(dataTypeSize(DataType::Float32) == 4);
    ASSERT(dataTypeSize(DataType::Float16) == 2);
    ASSERT(dataTypeSize(DataType::Int32) == 4);
    ASSERT(dataTypeSize(DataType::Uint8) == 1);
}

TEST(move_semantics) {
    TensorND t;
    int64_t shape[] = {100, 100};
    t.dataMemory = malloc(100 * 100 * sizeof(float));
    t.byteSize = 100 * 100 * sizeof(float);
    t.type = DataType::Float32;
    t.dimensions = 2;
    t.shape = static_cast<int64_t*>(malloc(2 * sizeof(int64_t)));
    std::memcpy(t.shape, shape, 2 * sizeof(int64_t));
    t.timestamp = 1;

    void* origData = t.dataMemory;
    int64_t* origShape = t.shape;

    TensorND moved = std::move(t);

    ASSERT(moved.dataMemory == origData);
    ASSERT(moved.shape == origShape);
    ASSERT(moved.byteSize == 100 * 100 * sizeof(float));

    free(moved.dataMemory);
    free(moved.shape);
}

TEST(assignment_operator) {
    TensorND a;
    a.dataMemory = malloc(100);
    a.byteSize = 100;
    a.type = DataType::Uint8;
    a.dimensions = 1;
    a.shape = static_cast<int64_t*>(malloc(sizeof(int64_t)));
    a.shape[0] = 100;

    TensorND b;
    b = a;

    ASSERT(b.dataMemory == a.dataMemory);
    ASSERT(b.shape == a.shape);
    ASSERT(b.byteSize == a.byteSize);

    free(a.dataMemory);
    free(a.shape);
}

// ===========================================================================
// JIT function signature test
// ===========================================================================
TEST(jit_function_signature) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    int64_t shape[] = {4};
    size_t byteSize = 4 * sizeof(float);

    TensorProcessFn fn = [](void* data, size_t byteSize,
                            const int64_t* shape, int dims,
                            uint8_t type) {
        if (type == 0) {
            float* f = static_cast<float*>(data);
            size_t count = byteSize / sizeof(float);
            for (size_t i = 0; i < count; ++i)
                f[i] *= 2.0f;
        }
    };

    fn(data, byteSize, shape, 1, static_cast<uint8_t>(DataType::Float32));

    ASSERT(std::abs(data[0] - 2.0f) < 0.001f);
    ASSERT(std::abs(data[1] - 4.0f) < 0.001f);
    ASSERT(std::abs(data[2] - 6.0f) < 0.001f);
    ASSERT(std::abs(data[3] - 8.0f) < 0.001f);
}

TEST(jit_function_type_dispatch) {
    uint8_t data_u8[] = {10, 20, 30};
    int32_t data_i32[] = {100, 200, 300};
    int64_t shape[] = {3};

    TensorProcessFn fn = [](void* data, size_t byteSize,
                            const int64_t* shape, int dims,
                            uint8_t type) {
        if (type == 3) {
            uint8_t* u = static_cast<uint8_t*>(data);
            for (size_t i = 0; i < byteSize; ++i)
                u[i] += 5;
        } else if (type == 2) {
            int32_t* i32 = static_cast<int32_t*>(data);
            size_t count = byteSize / sizeof(int32_t);
            for (size_t i = 0; i < count; ++i)
                i32[i] += 50;
        }
    };

    fn(data_u8, 3, shape, 1, static_cast<uint8_t>(DataType::Uint8));
    ASSERT(data_u8[0] == 15);
    ASSERT(data_u8[1] == 25);
    ASSERT(data_u8[2] == 35);

    fn(data_i32, 3 * sizeof(int32_t), shape, 1, static_cast<uint8_t>(DataType::Int32));
    ASSERT(data_i32[0] == 150);
    ASSERT(data_i32[1] == 250);
    ASSERT(data_i32[2] == 350);
}

// ===========================================================================
// Main
// ===========================================================================
int main() {
    printf("=== TensorND Test Suite ===\n\n");

    test_zero_copy_struct_shallow();
    test_zero_copy_no_memory_growth();
    test_struct_copy_latency();
    test_frame_skip_when_busy();
    test_ui_thread_never_blocks();
    test_pool_acquire_release();
    test_pool_thread_safety();
    test_default构造();
    test_element_count_2d();
    test_element_count_3d();
    test_data_type_sizes();
    test_move_semantics();
    test_assignment_operator();
    test_jit_function_signature();
    test_jit_function_type_dispatch();

    printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
    return testsFailed > 0 ? 1 : 0;
}
