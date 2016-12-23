// Copyright (c) 2016 dacci.org

#ifndef JUNO_NET_CHANNEL_H_
#define JUNO_NET_CHANNEL_H_

#include <winerror.h>

class __declspec(novtable) Channel {
 public:
  class __declspec(novtable) Listener {
   public:
    virtual ~Listener() {}

    virtual void OnRead(Channel* channel, HRESULT result, void* buffer,
                        int length) = 0;
    virtual void OnWritten(Channel* channel, HRESULT result, void* buffer,
                           int length) = 0;
  };

  virtual ~Channel() {}

  virtual void Close() = 0;
  virtual HRESULT ReadAsync(void* buffer, int length, Listener* listener) = 0;
  virtual HRESULT WriteAsync(const void* buffer, int length,
                             Listener* listener) = 0;
};

#ifndef JUNO_NO_CHANNEL_PTR
#include <memory>
typedef std::shared_ptr<Channel> ChannelPtr;
#endif  // JUNO_NO_CHANNEL_PTR

#endif  // JUNO_NET_CHANNEL_H_
