// Copyright (c) 2014 dacci.org

#include "io/channel_util.h"

#include <assert.h>

#include <windows.h>

#include <memory>

namespace juno {
namespace io {
namespace {

typedef struct {
  Channel::Listener* listener;
  ChannelEvent event;
  Channel* channel;
  HRESULT result;
  void* buffer;
  int length;
} EventData;

void FireEventImpl(Channel::Listener* listener, ChannelEvent event,
                   Channel* channel, HRESULT result, void* buffer, int length) {
  switch (event) {
    case Read:
      listener->OnRead(channel, result, buffer, length);
      break;

    case Write:
      listener->OnWritten(channel, result, buffer, length);
      break;

    default:
      assert(false);
  }
}

void CALLBACK FireEventImpl(PTP_CALLBACK_INSTANCE /*instance*/, void* param) {
  auto event_data = static_cast<EventData*>(param);

  FireEventImpl(event_data->listener, event_data->event, event_data->channel,
                event_data->result, event_data->buffer, event_data->length);

  delete event_data;
}

}  // namespace

namespace channel_util {

void FireEvent(Channel::Listener* listener, ChannelEvent event,
               Channel* channel, HRESULT result, const void* buffer,
               int length) {
  std::unique_ptr<EventData> event_data(new EventData{
      listener, event, channel, result, const_cast<void*>(buffer), length});

  if (event_data == nullptr)
    result = E_OUTOFMEMORY;
  else if (!TrySubmitThreadpoolCallback(FireEventImpl, event_data.get(),
                                        nullptr))
    result = HRESULT_FROM_WIN32(GetLastError());

  if (SUCCEEDED(result))
    event_data.release();
  else
    FireEventImpl(listener, event, channel, result, const_cast<void*>(buffer),
                  length);
}

}  // namespace channel_util
}  // namespace io
}  // namespace juno
