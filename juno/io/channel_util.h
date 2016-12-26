// Copyright (c) 2014 dacci.org

#ifndef JUNO_IO_CHANNEL_UTIL_H_
#define JUNO_IO_CHANNEL_UTIL_H_

#include "io/channel.h"

namespace juno {
namespace io {

enum ChannelEvent {
  Invalid,
  Read,
  Write,
};

namespace channel_util {

void FireEvent(Channel::Listener* listener, ChannelEvent event,
               Channel* channel, HRESULT result, const void* buffer,
               int length);

}  // namespace channel_util
}  // namespace io
}  // namespace juno

#endif  // JUNO_IO_CHANNEL_UTIL_H_
