// Copyright (c) 2013 dacci.org

#ifndef MADOKA_CONCURRENT_CRITICAL_SECTION_H_
#define MADOKA_CONCURRENT_CRITICAL_SECTION_H_

#include <madoka/common.h>
#include <madoka/concurrent/lockable.h>

namespace madoka {
namespace concurrent {

class CriticalSection : public Lockable {
 public:
  CriticalSection();
  virtual ~CriticalSection();

  virtual void Lock();
  virtual bool TryLock();
  virtual void Unlock();

 private:
  struct MutexImpl;

  MutexImpl* mutex_;

  DISALLOW_COPY_AND_ASSIGN(CriticalSection);
};

}  // namespace concurrent
}  // namespace madoka

#endif  // MADOKA_CONCURRENT_CRITICAL_SECTION_H_
