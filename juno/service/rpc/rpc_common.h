// Copyright (c) 2017 dacci.org

#ifndef JUNO_SERVICE_RPC_RPC_COMMON_H_
#define JUNO_SERVICE_RPC_RPC_COMMON_H_

#include <base/values.h>

namespace juno {
namespace service {
namespace rpc {

std::unique_ptr<base::DictionaryValue> CreateObject();
std::unique_ptr<base::DictionaryValue> CreateErrorObject(
    int code, const base::StringPiece& message);

namespace properties {

extern const char kVersion[];
extern const char kMethod[];
extern const char kParams[];
extern const char kId[];
extern const char kResult[];
extern const char kErrorCode[];
extern const char kErrorMessage[];
extern const char kErrorData[];

}  // namespace properties

namespace codes {

const int kParseError = -32700;
const int kInvalidRequest = -32600;
const int kMethodNotFound = -32601;
const int kInvalidParams = -32602;
const int kInternalError = -32603;
const int kServerError = -32000;

}  // namespace codes

namespace messages {

extern const char kParseError[];
extern const char kInvalidRequest[];
extern const char kMethodNotFound[];
extern const char kInvalidParams[];
extern const char kInternalError[];
extern const char kServerError[];

}  // namespace messages

namespace internal {

extern const char kVersion2_0[];

}  // namespace internal
}  // namespace rpc
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_RPC_RPC_COMMON_H_
