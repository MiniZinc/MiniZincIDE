#pragma once

#include <functional>
#include <chrono>
#include <QTimer>

namespace cpprofiler
{
namespace utils
{

class MaybeCaller : public QObject
{
  Q_OBJECT
private:
  int min_elapsed_; /// milliseconds

  std::chrono::system_clock::time_point last_call_time;

  QTimer updateTimer;

  std::function<void(void)> delayed_fn;

private Q_SLOTS:

  void callViaTimer();

public:
  /// the minimum duration between calls
  MaybeCaller(int min_ms);
  void call(std::function<void(void)> fn);
};

} // namespace utils
} // namespace cpprofiler
