// Copyright (c) 2015 dacci.org

#ifndef JUNO_MISC_TIMER_SERVICE_H_
#define JUNO_MISC_TIMER_SERVICE_H_

#include <stdint.h>

#include <windows.h>

#include <chrono>
#include <memory>

class TimerService {
 public:
  class __declspec(novtable) Callback {
   public:
    virtual ~Callback() {}

    virtual void OnTimeout() = 0;
  };

  class Timer {
   public:
    typedef std::ratio_multiply<std::hecto, std::nano> DelayRatio;
    typedef std::chrono::duration<int64_t, DelayRatio> Delay;
    typedef std::chrono::duration<DWORD, std::milli> Interval;

    ~Timer();

    // Starts this timer by specifying delay and interval in milli-seconds.
    void Start(DWORD delay, DWORD interval) {
      Start(std::chrono::milliseconds(delay), Interval(interval));
    }

    void Start(const Delay& delay, const Interval& interval);
    void Stop();

    bool IsStarted() const;

   private:
    friend class TimerService;

    Timer(PTP_TIMER timer, Callback* callback);

    const PTP_TIMER timer_;
    Callback* const callback_;
  };

  typedef std::shared_ptr<Timer> TimerObject;

  explicit TimerService(PTP_CALLBACK_ENVIRON environment);

  TimerObject Create(Callback* callback);

  static TimerService* GetDefault() {
    return &default_instance_;
  }

 private:
  static const DWORD kWindowLength = 10;

  static TimerService default_instance_;

  const PTP_CALLBACK_ENVIRON environment_;

  TimerService(const TimerService&) = delete;
  TimerService& operator=(const TimerService&) = delete;
};

#endif  // JUNO_MISC_TIMER_SERVICE_H_
