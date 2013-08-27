// Copyright (c) 2013 dacci.org

#include <madoka/concurrent/critical_section.h>

#include <windows.h>

namespace madoka {
namespace concurrent {

struct CriticalSection::MutexImpl : CRITICAL_SECTION {
};

CriticalSection::CriticalSection() : mutex_(new MutexImpl) {
  ::InitializeCriticalSection(mutex_);
}

CriticalSection::~CriticalSection() {
  ::DeleteCriticalSection(mutex_);
  delete mutex_;
}

void CriticalSection::Lock() {
  ::EnterCriticalSection(mutex_);
}

bool CriticalSection::TryLock() {
  return ::TryEnterCriticalSection(mutex_) != FALSE;
}

void CriticalSection::Unlock() {
  ::LeaveCriticalSection(mutex_);
}

}  // namespace concurrent
}  // namespace madoka
