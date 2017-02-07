// Copyright (c) 2017 dacci.org

#ifndef JUNO_SERVICE_RPC_RPC_CLIENT_H_
#define JUNO_SERVICE_RPC_RPC_CLIENT_H_

#include <base/values.h>
#include <base/synchronization/lock.h>

#include <map>
#include <memory>

#include "io/named_pipe_channel.h"

namespace juno {
namespace service {
namespace rpc {

class RpcClient : private io::NamedPipeChannel::Listener {
 public:
  class __declspec(novtable) Listener {
   public:
    virtual ~Listener() {}

    virtual void OnReturned(const char* method, const base::Value* result,
                            const base::DictionaryValue* error) = 0;
  };

  RpcClient();
  ~RpcClient();

  HRESULT Connect(const base::StringPiece16& name);
  void Close();

  HRESULT CallMethod(const base::StringPiece& method, const base::Value* params,
                     Listener* listener);

 private:
  struct Session;

  static const int kBufferSize = 1024;

  void OnConnected(io::NamedPipeChannel* channel, HRESULT result) override;
  void OnTransacted(io::NamedPipeChannel* channel, HRESULT result, void* input,
                    void* output, int length) override;

  LONG id_;
  base::Lock lock_;
  std::map<void*, std::unique_ptr<Session>> sessions_;
  io::NamedPipeChannel channel_;

  RpcClient(const RpcClient&) = delete;
  RpcClient& operator=(const RpcClient&) = delete;
};

}  // namespace rpc
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_RPC_RPC_CLIENT_H_
