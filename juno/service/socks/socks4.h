// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS4_H_
#define JUNO_SERVICE_SOCKS_SOCKS4_H_

#include <stdint.h>

#include <ws2tcpip.h>

namespace SOCKS4 {

enum COMMAND : uint8_t {
  CONNECT = 1,
  BIND,
};

enum CODE : uint8_t {
  GRANTED = 90,
  FAILED,
  IDENT_FAILED,
  IDENT_MISMATCH,
};

#include <pshpack1.h>

typedef struct REQUEST {
  uint8_t version;
  COMMAND command;
  uint16_t port;
  in_addr address;
  char user_id[1];
} REQUEST;

typedef struct RESPONSE {
  uint8_t version;
  CODE code;
  uint16_t port;
  in_addr address;
} RESPONSE;

#include <poppack.h>

}  // namespace SOCKS4

#endif  // JUNO_SERVICE_SOCKS_SOCKS4_H_
