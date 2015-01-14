// Copyright (c) 2015 dacci.org

#include "misc/timer_service.h"

namespace {

void CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE instance, void* context,
                            PTP_TIMER timer) {
  static_cast<TimerService::Callback*>(context)->OnTimeout();
}

}  // namespace

TimerService TimerService::default_instance_(nullptr);

TimerService::TimerService(PTP_CALLBACK_ENVIRON environment)
    : environment_(environment) {
}

TimerService::TimerObject TimerService::Create(Callback* callback) {
  PTP_TIMER timer = CreateThreadpoolTimer(TimerCallback, callback,
                                          environment_);
  if (timer == nullptr)
    return nullptr;

  TimerObject timer_object(new Timer(timer, callback));
  if (timer_object == nullptr)
    CloseThreadpoolTimer(timer);

  return timer_object;
}

TimerService::Timer::~Timer() {
  Stop();
  WaitForThreadpoolTimerCallbacks(timer_, FALSE);
  CloseThreadpoolTimer(timer_);
}

void TimerService::Timer::Start(const Delay& delay, const Interval& interval) {
  LARGE_INTEGER large_integer;
  large_integer.QuadPart = -delay.count();

  FILETIME file_time;
  file_time.dwLowDateTime = large_integer.LowPart;
  file_time.dwHighDateTime = large_integer.HighPart;

  SetThreadpoolTimer(timer_, &file_time, interval.count(), kWindowLength);
}

void TimerService::Timer::Stop() {
  SetThreadpoolTimer(timer_, nullptr, 0, 0);
}

bool TimerService::Timer::IsStarted() const {
  return IsThreadpoolTimerSet(timer_) != FALSE;
}

TimerService::Timer::Timer(PTP_TIMER timer, Callback* callback)
    : timer_(timer), callback_(callback) {
}
