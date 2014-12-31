// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_CHANNEL_UTIL_H_
#define JUNO_NET_CHANNEL_UTIL_H_

#include "net/channel.h"

enum ChannelEvent {
  Invalid,
  Read,
  Write,
};

namespace channel_util {

void FireEvent(Channel::Listener* listener, ChannelEvent event,
               Channel* channel, DWORD error, const void* buffer, int length);

}  // namespace channel_util

#endif  // JUNO_NET_CHANNEL_UTIL_H_
