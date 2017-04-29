// Copyright (c) 2017 dacci.org

#include "service/rpc/rpc_service.h"

#include <base/logging.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>

#include <memory>
#include <string>

#include "service/rpc/rpc_common.h"

namespace juno {
namespace service {
namespace rpc {

RpcService::RpcService() {}

HRESULT RpcService::Start(const base::StringPiece16& name) {
  auto result =
      channel_.Create(name, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
                      PIPE_UNLIMITED_INSTANCES, NMPWAIT_USE_DEFAULT_WAIT);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to create channel: 0x" << std::hex << result;
    return result;
  }

  result = channel_.ConnectAsync(this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to wait client: 0x" << std::hex << result;
    return result;
  }

  return S_OK;
}

void RpcService::Stop() {
  channel_.Close();
}

void RpcService::RegisterMethod(const std::string& name, MethodType method,
                                void* context) {
  base::AutoLock guard(lock_);

  methods_[name] = method;
  contexts_[name] = context;
}

void RpcService::UnregisterMethod(const std::string& name) {
  base::AutoLock guard(lock_);

  methods_.erase(name);
  contexts_.erase(name);
}

std::unique_ptr<base::DictionaryValue> RpcService::ProcessRequest(
    const base::DictionaryValue* request) {
  DCHECK(request != nullptr);

  std::string jsonrpc;
  auto has_jsonrpc = request->GetString(properties::kVersion, &jsonrpc);
  std::string method;
  auto has_method = request->GetString(properties::kMethod, &method);
  const base::Value* id = nullptr;
  auto has_id = request->Get(properties::kId, &id);
  auto id_type = has_id ? id->GetType() : base::Value::Type::NONE;
  const base::Value* params = nullptr;
  auto has_params = request->Get(properties::kParams, &params);
  auto params_type = has_params ? params->GetType() : base::Value::Type::NONE;

  if (!has_jsonrpc || !has_method ||
      has_id && id_type != base::Value::Type::STRING &&
          id_type != base::Value::Type::INTEGER &&
          id_type != base::Value::Type::DOUBLE &&
          id_type != base::Value::Type::NONE ||
      has_params && params_type != base::Value::Type::DICTIONARY &&
          params_type != base::Value::Type::LIST ||
      jsonrpc.compare(internal::kVersion2_0) != 0)
    return CreateErrorObject(codes::kInvalidRequest, messages::kInvalidRequest);

  std::unique_ptr<base::DictionaryValue> response;
  if (has_id) {
    response = CreateObject();
    response->Set(properties::kId, id->CreateDeepCopy());
  }

  {
    base::AutoLock guard(lock_);

    auto method_entry = methods_.find(method);
    if (method_entry != methods_.end()) {
      auto context = contexts_[method];
      method_entry->second(context, params, response.get());
    } else if (has_id) {
      response->SetInteger(properties::kErrorCode, codes::kMethodNotFound);
      response->SetString(properties::kErrorMessage, messages::kMethodNotFound);
    }
  }

  return std::move(response);
}

std::unique_ptr<base::Value> RpcService::ProcessBatch(
    const base::ListValue* requests) {
  DCHECK(requests != nullptr);

  if (requests->empty())
    return CreateErrorObject(codes::kInvalidRequest, messages::kInvalidRequest);

  auto responses = std::make_unique<base::ListValue>();

  for (auto& entry : *requests) {
    const base::DictionaryValue* request = nullptr;
    if (entry.GetAsDictionary(&request)) {
      auto response = ProcessRequest(request);
      if (response != nullptr)
        responses->Append(std::move(response));
    } else {
      responses->Append(
          CreateErrorObject(codes::kInvalidRequest, messages::kInvalidRequest));
    }
  }

  if (responses->empty())
    return nullptr;

  return std::move(responses);
}

void RpcService::OnRead(io::Channel* channel, HRESULT result, void* memory,
                        int length) {
  auto pipe = static_cast<io::NamedPipeChannel*>(channel);
  std::unique_ptr<char[]> buffer(static_cast<char*>(memory));

  do {
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to read: 0x" << std::hex << result;
      break;
    }

    std::string message(buffer.get(), length);

    while (result == ERROR_MORE_DATA) {
      length = kBufferSize;
      result = pipe->Read(buffer.get(), &length);
      message.append(buffer.get(), length);
    }
    if (FAILED(result))
      break;

    std::unique_ptr<base::Value> response;

    auto json = base::JSONReader::Read(message);
    if (json != nullptr) {
      switch (json->GetType()) {
        case base::Value::Type::DICTIONARY: {
          const base::DictionaryValue* request = nullptr;
          json->GetAsDictionary(&request);
          response = ProcessRequest(request);
          break;
        }

        case base::Value::Type::LIST: {
          const base::ListValue* requests = nullptr;
          json->GetAsList(&requests);
          response = ProcessBatch(requests);
          break;
        }

        default:
          response = CreateErrorObject(codes::kInvalidRequest,
                                       messages::kInvalidRequest);
          break;
      }
    } else {
      response = CreateErrorObject(codes::kParseError, messages::kParseError);
    }

    if (response != nullptr) {
      if (!base::JSONWriter::Write(*response, &message)) {
        LOG(ERROR) << "Failed to serialize response.";
        break;
      }

      length = static_cast<int>(message.size());
      buffer = std::make_unique<char[]>(length);
      memcpy(buffer.get(), message.data(), length);
    } else {
      length = 0;
      buffer.reset();
    }

    result = pipe->WriteAsync(buffer.get(), length, this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to write: 0x" << std::hex << result;
      break;
    }

    buffer.release();
    return;
  } while (false);

  do {
    result = pipe->Disconnect();
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to disconnect: 0x" << std::hex << result;
      break;
    }

    result = pipe->ConnectAsync(this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to wait client: 0x" << std::hex << result;
      break;
    }

    return;
  } while (false);

  Stop();
}

void RpcService::OnWritten(io::Channel* channel, HRESULT result, void* memory,
                           int /*length*/) {
  auto pipe = static_cast<io::NamedPipeChannel*>(channel);
  static_cast<std::unique_ptr<char[]>>(static_cast<char*>(memory));

  do {
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to write: 0x" << std::hex << result;
    } else {
      auto buffer = std::make_unique<char[]>(kBufferSize);
      if (buffer == nullptr) {
        LOG(ERROR) << "Failed to allocate buffer.";
        break;
      }

      result = pipe->ReadAsync(buffer.get(), kBufferSize, this);
      if (FAILED(result)) {
        LOG(ERROR) << "Failed to read: 0x" << std::hex << result;
      } else {
        buffer.release();
        return;
      }
    }

    result = pipe->Disconnect();
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to disconnect: 0x" << std::hex << result;
      break;
    }

    result = pipe->ConnectAsync(this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to wait client: 0x" << std::hex << result;
      break;
    }

    return;
  } while (false);

  Stop();
}

void RpcService::OnConnected(io::NamedPipeChannel* pipe, HRESULT result) {
  do {
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to wait client: 0x" << std::hex << result;
      break;
    }

    auto buffer = std::make_unique<char[]>(kBufferSize);
    if (buffer == nullptr) {
      LOG(ERROR) << "Failed to allocate buffer.";
      break;
    }

    result = pipe->ReadAsync(buffer.get(), kBufferSize, this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to read: 0x" << std::hex << result;
    } else {
      buffer.release();
      return;
    }

    result = pipe->Disconnect();
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to disconnect: 0x" << std::hex << result;
      break;
    }

    result = pipe->ConnectAsync(this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to wait client: 0x" << std::hex << result;
      break;
    }

    return;
  } while (false);

  Stop();
}

void RpcService::OnTransacted(io::NamedPipeChannel* pipe, HRESULT result,
                              void* input, void* output, int length) {
  LOG(FATAL) << "pipe: " << pipe << ", result: 0x" << std::hex << result
             << ", input: " << input << ", output: " << output
             << ", length: " << std::dec << length;
}

}  // namespace rpc
}  // namespace service
}  // namespace juno
