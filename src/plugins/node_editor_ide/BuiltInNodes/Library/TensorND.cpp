#include "TensorND.h"
#include <cstdlib>
#include <cstring>
#include <cassert>

namespace Daqster {

// ---------------------------------------------------------------------------
// Helpers (defined before TensorPool so they are visible)
// ---------------------------------------------------------------------------
static TensorND allocateTensor(DataType type, const int64_t* shape, int dimensions) {
    TensorND t;
    t.type = type;
    t.dimensions = dimensions;
    t.timestamp = 0;

    t.shape = static_cast<int64_t*>(malloc(dimensions * sizeof(int64_t)));
    std::memcpy(t.shape, shape, dimensions * sizeof(int64_t));

    t.byteSize = dataTypeSize(type);
    for (int i = 0; i < dimensions; ++i)
        t.byteSize *= static_cast<size_t>(shape[i]);

    t.dataMemory = calloc(1, t.byteSize);
    return t;
}

static void freeTensor(TensorND& t) {
    free(t.dataMemory);
    free(t.shape);
    t.dataMemory = nullptr;
    t.shape = nullptr;
    t.byteSize = 0;
    t.dimensions = 0;
}

// ---------------------------------------------------------------------------
// TensorPool
// ---------------------------------------------------------------------------
TensorPool::TensorPool(DataType type, const int64_t* shape, int dims, size_t poolSize) {
    m_pool.reserve(poolSize);
    for (size_t i = 0; i < poolSize; ++i) {
        auto* buf = new Buffer();
        buf->tensor = allocateTensor(type, shape, dims);
        m_pool.push_back(buf);
    }
}

TensorPool::~TensorPool() {
    for (auto* buf : m_pool) {
        if (buf) {
            freeTensor(buf->tensor);
            delete buf;
        }
    }
    m_pool.clear();
}

TensorPool::Buffer* TensorPool::acquire() {
    for (auto* buf : m_pool) {
        bool expected = false;
        if (buf->inUse.compare_exchange_strong(expected, true)) {
            return buf;
        }
    }
    return nullptr;
}

void TensorPool::release(Buffer* buf) {
    if (buf) {
        buf->inUse.store(false);
    }
}

bool TensorPool::allFree() const {
    for (auto* buf : m_pool) {
        if (buf->inUse.load())
            return false;
    }
    return true;
}

} // namespace Daqster
