// Copyright (c) 2017 dacci.org

#include "service/rpc/rpc_common.h"

namespace juno {
namespace service {
namespace rpc {

std::unique_ptr<base::DictionaryValue> CreateObject() {
  auto object = std::make_unique<base::DictionaryValue>();
  object->SetString(properties::kVersion, internal::kVersion2_0);
  return std::move(object);
}

std::unique_ptr<base::DictionaryValue> CreateErrorObject(
    int code, const base::StringPiece& message) {
  auto object = CreateObject();
  object->SetInteger(properties::kErrorCode, code);
  object->SetString(properties::kErrorMessage, message.as_string());
  object->Set(properties::kId, base::Value::CreateNullValue());
  return std::move(object);
}

namespace properties {

const char kVersion[] = "jsonrpc";
const char kMethod[] = "method";
const char kParams[] = "params";
const char kId[] = "id";
const char kResult[] = "result";
const char kErrorCode[] = "error.code";
const char kErrorMessage[] = "error.message";
const char kErrorData[] = "error.data";

}  // namespace properties

namespace messages {

const char kParseError[] = "Parse error";
const char kInvalidRequest[] = "Invalid Request";
const char kMethodNotFound[] = "Method not found";
const char kInvalidParams[] = "Invalid params";
const char kInternalError[] = "Internal error";
const char kServerError[] = "Server error";

}  // namespace messages

namespace internal {

const char kVersion2_0[] = "2.0";

}  // namespace internal
}  // namespace rpc
}  // namespace service
}  // namespace juno
