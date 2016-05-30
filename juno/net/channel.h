// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_CHANNEL_H_
#define JUNO_NET_CHANNEL_H_

#include <windows.h>

class Channel {
 public:
  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnRead(Channel* channel, DWORD error, void* buffer,
                        int length) = 0;
    virtual void OnWritten(Channel* channel, DWORD error, void* buffer,
                           int length) = 0;
  };

  virtual ~Channel() {}

  virtual void Close() = 0;
  virtual void ReadAsync(void* buffer, int length, Listener* listener) = 0;
  virtual void WriteAsync(const void* buffer, int length,
                          Listener* listener) = 0;
};

#endif  // JUNO_NET_CHANNEL_H_
