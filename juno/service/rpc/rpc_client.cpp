// Copyright (c) 2017 dacci.org

#include "service/rpc/rpc_client.h"

#include <base/logging.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>

#include <memory>
#include <string>

#include "service/rpc/rpc_common.h"

namespace juno {
namespace service {
namespace rpc {

struct RpcClient::Session {
  std::string method;
  Listener* listener;
};

RpcClient::RpcClient() : id_(0) {}

RpcClient::~RpcClient() {
  Close();
}

HRESULT RpcClient::Connect(const base::StringPiece16& name) {
  base::AutoLock guard(lock_);

  if (channel_.IsValid()) {
    LOG(ERROR) << "Already connected.";
    return E_HANDLE;
  }

  auto result = channel_.Open(name, NMPWAIT_USE_DEFAULT_WAIT);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to open: 0x" << std::hex << result;
    return result;
  }

  result = channel_.SetMode(PIPE_READMODE_MESSAGE);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to set mode: 0x" << std::hex << result;
    return result;
  }

  return S_OK;
}

void RpcClient::Close() {
  channel_.Close();
}

HRESULT RpcClient::CallMethod(const base::StringPiece& method,
                              const base::Value* params, Listener* listener) {
  if (method.empty())
    return E_INVALIDARG;

  auto request = CreateObject();
  if (request == nullptr) {
    LOG(ERROR) << "Failed to create request object.";
    return E_OUTOFMEMORY;
  }

  auto session = std::make_unique<Session>();
  if (session == nullptr) {
    LOG(ERROR) << "Failed to create session.";
    return E_OUTOFMEMORY;
  }

  session->method = method.as_string();
  session->listener = listener;

  request->SetInteger(properties::kId, InterlockedIncrement(&id_));
  request->SetString(properties::kMethod, session->method);

  if (params != nullptr) {
    auto copy = params->CreateDeepCopy();
    if (copy == nullptr) {
      LOG(ERROR) << "Failed to copy params.";
      return E_OUTOFMEMORY;
    }

    request->Set(properties::kParams, std::move(copy));
  }

  std::string json;
  if (!base::JSONWriter::Write(*request, &json)) {
    LOG(ERROR) << "Failed to serialize.";
    return E_FAIL;
  }

  auto input = std::make_unique<char[]>(json.size());
  auto output = std::make_unique<char[]>(kBufferSize);
  if (input == nullptr || output == nullptr) {
    LOG(ERROR) << "Failed to allocate buffer.";
    return E_OUTOFMEMORY;
  }

  memcpy(input.get(), json.data(), json.size());

  base::AutoLock guard(lock_);

  if (!channel_.IsValid())
    return E_HANDLE;

  auto result =
      channel_.TransactAsync(input.get(), static_cast<int>(json.size()),
                             output.get(), kBufferSize, this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to transact: 0x" << std::hex << result;
    return result;
  }

  sessions_[input.get()] = std::move(session);
  input.release();
  output.release();

  return S_OK;
}

void RpcClient::OnConnected(io::NamedPipeChannel* channel, HRESULT result) {
  LOG(FATAL) << "channel: " << channel << ", result: 0x" << std::hex << result;
}

void RpcClient::OnTransacted(io::NamedPipeChannel* channel, HRESULT result,
                             void* input, void* output, int length) {
  std::unique_ptr<char[]>(static_cast<char*>(input));
  std::unique_ptr<char[]> buffer(static_cast<char*>(output));
  std::string json;

  lock_.Acquire();

  auto iter = sessions_.find(input);
  auto session = std::move(iter->second);
  sessions_.erase(iter);

  do {
    base::AutoLock guard(lock_, base::AutoLock::AlreadyAcquired());

    if (FAILED(result)) {
      LOG(ERROR) << "Failed to transact: 0x" << std::hex << result;
      break;
    }

    json.assign(buffer.get(), length);

    while (result == ERROR_MORE_DATA) {
      length = kBufferSize;
      result = channel->Read(buffer.get(), &length);
      json.append(buffer.get(), length);
    }
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to read: 0x" << std::hex << result;
      break;
    }
  } while (false);

  do {
    if (FAILED(result))
      break;

    if (session->listener == nullptr)
      return;

    auto response = base::DictionaryValue::From(base::JSONReader::Read(json));
    if (response == nullptr) {
      LOG(ERROR) << "Failed to parse JSON.";
      break;
    }

    const base::Value* method_result = nullptr;
    response->Get("result", &method_result);
    const base::DictionaryValue* error = nullptr;
    response->GetDictionary("error", &error);

    session->listener->OnReturned(session->method.c_str(), method_result,
                                  error);
    return;
  } while (false);

  channel->Close();
}

}  // namespace rpc
}  // namespace service
}  // namespace juno
