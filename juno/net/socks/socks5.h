// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SOCKS_SOCKS5_H_
#define JUNO_NET_SOCKS_SOCKS5_H_

#include <stdint.h>

#include <ws2tcpip.h>

namespace SOCKS5 {

enum METHOD : uint8_t {
  NO_AUTH = 0,
  GSSAPI,
  PASSWORD,
  UNSUPPORTED = 0xFF,
};

enum COMMAND : uint8_t {
  CONNECT = 1,
  BIND,
  UDP_ASSOCIATE,
};

enum ATYPE : uint8_t {
  IP_V4 = 1,
  UNUSED,
  DOMAINNAME,
  IP_V6,
};

enum CODE : uint8_t {
  SUCCEEDED = 0,
  GENERAL_FAILURE,
  CLIENT_REJECTED,
  NETWORK_UNREACHABLE,
  HOST_UNREACHABLE,
  CONNECTION_REFUSED,
  TTL_EXPIRED,
  COMMAND_NOT_SUPPORTED,
  ADDRESS_TYPE_NOT_SUPPORTED,
};

#pragma pack(push, 1)

typedef struct METHOD_REQUEST {
  uint8_t version;
  uint8_t method_count;
  METHOD methods[1];
} METHOD_REQUEST;

typedef struct METHOD_RESPONSE {
  uint8_t version;
  METHOD method;
} METHOD_RESPONSE;

typedef union ADDRESS {
  struct {
    in_addr ipv4_addr;
    uint16_t ipv4_port;
  } ipv4;
  struct {
    uint8_t domain_len;
    char domain_name[1];
  } domain;
  struct {
    in6_addr ipv6_addr;
    uint16_t ipv6_port;
  } ipv6;
} ADDRESS;

typedef struct REQUEST {
  uint8_t version;
  COMMAND command;
  uint8_t reserved;
  ATYPE type;
  ADDRESS address;
} REQUEST;

typedef struct RESPONSE {
  uint8_t version;
  CODE code;
  uint8_t reserved;
  ATYPE type;
  ADDRESS address;
} RESPONSE;

#pragma pack(pop)

}  // namespace SOCKS5

#endif  // JUNO_NET_SOCKS_SOCKS5_H_
