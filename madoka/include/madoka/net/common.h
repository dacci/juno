// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_COMMON_H_
#define MADOKA_NET_COMMON_H_

#include <madoka/common.h>

#ifdef WIN32
  #include <ws2tcpip.h>
  #include <mswsock.h>

  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "mswsock.lib")
  #endif  // _MSC_VER

  #define SHUT_RD   SD_RECEIVE
  #define SHUT_WR   SD_SEND
  #define SHUT_RDWR SD_BOTH
#else   // WIN32
  #include <unistd.h>
  #include <sys/ioctl.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>

  typedef int SOCKET;

  #define SOCKET_ERROR (-1)

  #define INVALID_SOCKET (-1)

  #define SD_RECEIVE  SHUT_RD
  #define SD_SEND     SHUT_WR
  #define SD_BOTH     SHUT_RDWR

  #define closesocket close
  #define ioctlsocket ioctl
#endif  // WIN32

#endif  // MADOKA_NET_COMMON_H_
