#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>
#include <functional>

namespace Daqster {

    enum class DataType : uint8_t {
        Float32 = 0,
        Float16 = 1,
        Int32   = 2,
        Uint8   = 3
    };

    inline size_t dataTypeSize(DataType t) {
        switch (t) {
            case DataType::Float32: return sizeof(float);
            case DataType::Float16: return sizeof(uint16_t);
            case DataType::Int32:   return sizeof(int32_t);
            case DataType::Uint8:   return sizeof(uint8_t);
        }
        return sizeof(float);
    }

    struct TensorND {
        void*    dataMemory   = nullptr;
        size_t   byteSize     = 0;
        DataType type         = DataType::Float32;
        int      dimensions   = 0;
        int64_t* shape        = nullptr;
        uint64_t timestamp    = 0;

        TensorND() = default;
        TensorND(const TensorND&) = default;
        TensorND& operator=(const TensorND&) = default;
        TensorND(TensorND&&) noexcept = default;
        TensorND& operator=(TensorND&&) noexcept = default;

        bool isValid() const {
            return dataMemory != nullptr && byteSize > 0 && shape != nullptr && dimensions > 0;
        }

        size_t elementCount() const {
            if (dimensions <= 0 || !shape) return 0;
            size_t count = 1;
            for (int i = 0; i < dimensions; ++i)
                count *= static_cast<size_t>(shape[i]);
            return count;
        }
    };

    using TensorProcessFn = void (*)(void* data, size_t byteSize,
                                     const int64_t* shape, int dims,
                                     uint8_t type);

    class TensorPool {
    public:
        struct Buffer {
            TensorND    tensor;
            std::atomic<bool> inUse{false};
        };

        explicit TensorPool(DataType type, const int64_t* shape, int dims, size_t poolSize = 3);
        ~TensorPool();

        TensorPool(const TensorPool&) = delete;
        TensorPool& operator=(const TensorPool&) = delete;

        Buffer* acquire();
        void release(Buffer* buf);
        bool allFree() const;
        size_t size() const { return m_pool.size(); }

    private:
        std::vector<Buffer*> m_pool;
    };

} // namespace Daqster
