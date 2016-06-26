// Copyright (c) 2013 dacci.org

#include "service/http/http_status.h"

namespace HTTP {
namespace {

typedef struct STATUS_MESSAGE {
  int status;
  const char* message;
} STATUS_MESSAGE;

// clang-format off
STATUS_MESSAGE http_status_messages[] = {
  // 1xx Informational,
  { CONTINUE, "Continue" },
  { SWITCHING_PROTOCOLS, "Switching Protocols" },

  // 2xx Success
  { OK, "OK" },
  { CREATED, "Created" },
  { ACCEPTED, "Accepted" },
  { NON_AUTHORITATIVE_INFORMATION, "Non-Authoritative Information" },
  { NO_CONTENT, "No Content" },
  { RESET_CONTENT, "Reset Content" },
  { PARTIAL_CONTENT, "Partial Content" },
  { IM_USED, "IM Used" },
  { LOW_ON_STORAGE_SPACE, "Low on Storage Space" },

  // 3xx Redirection
  { MULTIPLE_CHOICES, "Multiple Choices" },
  { MOVED_PERMANENTLY, "Moved Permanently" },
  { FOUND, "Found" },
  { SEE_OTHER, "See Other" },
  { NOT_MODIFIED, "Not Modified" },
  { USE_PROXY, "Use Proxy" },
  { SWITCH_PROXY, "Switch Proxy" },
  { TEMPORARY_REDIRECT, "Temporary Redirect" },
  { PERMANENT_REDIRECT, "Permanent Redirect" },

  // 4xx Client Error
  { BAD_REQUEST, "Bad Request" },
  { UNAUTHORIZED, "Unauthorized" },
  { PAYMENT_REQUIRED, "Payment Required" },
  { FORBIDDEN, "Forbidden" },
  { NOT_FOUND, "Not Found" },
  { METHOD_NOT_ALLOWED, "Method Not Allowed" },
  { NOT_ACCEPTABLE, "Not Acceptable" },
  { PROXY_AUTHENTICATION_REQUIRED, "Proxy Authentication Required" },
  { REQUEST_TIMEOUT, "Request Timeout" },
  { CONFLICT, "Conflict" },
  { GONE, "Gone" },
  { LENGTH_REQUIRED, "Length Required" },
  { PRECONDITION_FAILED, "Precondition Failed" },
  { REQUEST_ENTITY_TOO_LARGE, "Request Entity Too Large" },
  { REQUEST_URI_TOO_LONG, "Request-URI Too Long" },
  { UNSUPPORTED_MEDIA_TYPE, "Unsupported Media Type" },
  { REQUESTED_RANGE_NOT_SATISFIABLE, "Requested Range Not Satisfiable" },
  { EXPECTATION_FAILED, "Expectation Failed" },
  { I_AM_A_TEAPOT, "I'm a teapot" },
  { UNORDERED_COLLECTION, "Unordered Collection" },
  { UPGRADE_REQUIRED, "Upgrade Required" },
  { PRECONDITION_REQUIRED, "Precondition Required" },
  { TOO_MANY_REQUESTS, "Too Many Requests" },
  { REQUEST_HEADER_FIELDS_TOO_LARGE, "Request Header Fields Too Large" },
  { UNAVAILABLE_FOR_LEGAL_REASONS, "Unavailable For Legal Reasons" },

  // 5xx Server Error
  { INTERNAL_SERVER_ERROR, "Internal Server Error" },
  { NOT_IMPLEMENTED, "Not Implemented" },
  { BAD_GATEWAY, "Bad Gateway" },
  { SERVICE_UNAVAILABLE, "Service Unavailable" },
  { GATEWAY_TIMEOUT, "Gateway Timeout" },
  { HTTP_VERSION_NOT_SUPPORTED, "HTTP Version Not Supported" },
  { VARIANT_ALSO_NEGOTIATES, "Variant Also Negotiates" },
  { BANDWIDTH_LIMIT_EXCEEDED, "Bandwidth Limit Exceeded" },
  { NOT_EXTENDED, "Not Extended" },
  { NETWORK_AUTHENTICATION_REQUIRED, "Network Authentication Required" },
  { PERMISSION_DENIED, "Permission denied" },
  { 0, nullptr }
};
// clang-format on

}  // namespace

const char* GetStatusMessage(StatusCode status) {
  auto entry = http_status_messages;

  while (entry->status) {
    if (entry->status == status)
      break;

    ++entry;
  }

  return entry->message;
}

}  // namespace HTTP
