#ifndef DAQ_GLOBAL_H
#define DAQ_GLOBAL_H

#include "build_cfg.h"

namespace Daqster {

enum class ErrorCode : int {
    NoError = 0,
    InitError,
    WrongParams,
    WrongData,
    NullPointer,
    SomeError
};

using pack_id_t = int;
using msg_id_t = int;

} // namespace Daqster

// Backward compatibility aliases
using Daqster::ErrorCode;
constexpr auto NO_ERR = ErrorCode::NoError;
constexpr auto INIT_ERROR = ErrorCode::InitError;
constexpr auto WRONG_PARAMS = ErrorCode::WrongParams;
constexpr auto WRONG_DATA = ErrorCode::WrongData;
constexpr auto NULL_POINTER = ErrorCode::NullPointer;
constexpr auto SOME_ERROR = ErrorCode::SomeError;

#endif
