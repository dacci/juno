// Copyright (c) 2013 dacci.org

#ifndef BASE_LOGGING_H_
#define BASE_LOGGING_H_

namespace logging {

class LogMessage {
 public:
  void operator<<(const char*) {
  }
};

}  // namespace logging

#define DCHECK(cond) true ? (void)0 : logging::LogMessage()
#define DCHECK_NE(val1, val2)  true ? (void)0 : logging::LogMessage()
#define DCHECK_LT(val1, val2)  true ? (void)0 : logging::LogMessage()
#define DCHECK_GT(val1, val2)  true ? (void)0 : logging::LogMessage()

#define NOTREACHED()

#endif  // BASE_LOGGING_H_
