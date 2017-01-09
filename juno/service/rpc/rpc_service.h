// Copyright (c) 2017 dacci.org

#ifndef JUNO_SERVICE_RPC_RPC_SERVICE_H_
#define JUNO_SERVICE_RPC_RPC_SERVICE_H_

#include <base/values.h>

#include <base/synchronization/lock.h>

#include <map>
#include <string>

#include "io/named_pipe_channel.h"

namespace juno {
namespace service {
namespace rpc {

class RpcService : private io::Channel::Listener,
                   private io::NamedPipeChannel::Listener {
 public:
  typedef void (*MethodType)(void* context, const base::Value* params,
                             base::DictionaryValue* response);

  RpcService();

  HRESULT Start(const base::StringPiece16& name);
  void Stop();

  void RegisterMethod(const std::string& name, MethodType method,
                      void* context);
  void UnregisterMethod(const std::string& name);

 private:
  static const int kBufferSize = 1024;

  std::unique_ptr<base::DictionaryValue> ProcessRequest(
      const base::DictionaryValue* request);
  std::unique_ptr<base::Value> ProcessBatch(const base::ListValue* requests);

  void OnRead(io::Channel* channel, HRESULT result, void* memory,
              int length) override;
  void OnWritten(io::Channel* channel, HRESULT result, void* memory,
                 int length) override;
  void OnConnected(io::NamedPipeChannel* pipe, HRESULT result) override;
  void OnTransacted(io::NamedPipeChannel* pipe, HRESULT result, void* input,
                    void* output, int length) override;

  io::NamedPipeChannel channel_;
  base::Lock lock_;
  std::map<std::string, MethodType> methods_;
  std::map<std::string, void*> contexts_;

  RpcService(const RpcService&) = delete;
  RpcService& operator=(const RpcService&) = delete;
};

}  // namespace rpc
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_RPC_RPC_SERVICE_H_
