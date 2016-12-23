// Copyright (c) 2016 dacci.org

#ifndef JUNO_NET_DATAGRAM_CHANNEL_H_
#define JUNO_NET_DATAGRAM_CHANNEL_H_

#include <base/synchronization/lock.h>

#include <list>
#include <memory>

#include "net/channel.h"
#include "net/socket.h"

class DatagramChannel : public Socket, public Channel {
 public:
  class __declspec(novtable) Listener {
   public:
    virtual ~Listener() {}

    virtual void OnRead(DatagramChannel* channel, HRESULT result, void* buffer,
                        int length, const void* from, int from_length) = 0;
  };

  DatagramChannel();
  virtual ~DatagramChannel();

  void Close() override;
  HRESULT ReadAsync(void* buffer, int length,
                    Channel::Listener* listener) override;
  HRESULT ReadFromAsync(void* buffer, int length, Listener* listener);
  HRESULT WriteAsync(const void* buffer, int length,
                     Channel::Listener* listener) override;
  HRESULT WriteAsync(const void* buffer, int length, const void* address,
                     size_t address_length, Channel::Listener* listener);

 private:
  struct Request;

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE callback,
                                   void* instance, PTP_WORK work);
  void OnRequested(PTP_WORK work);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE callback,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR bytes, PTP_IO io);

  base::Lock lock_;
  PTP_WORK work_;
  std::list<std::unique_ptr<Request>> queue_;
  PTP_IO io_;

  DatagramChannel(const DatagramChannel&) = delete;
  DatagramChannel& operator=(const DatagramChannel&) = delete;
};

#endif  // JUNO_NET_DATAGRAM_CHANNEL_H_
