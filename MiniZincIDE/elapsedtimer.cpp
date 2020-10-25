#include "elapsedtimer.h"

ElapsedTimer::ElapsedTimer(QObject* parent) : QObject(parent) {
    connect(&_interval, &QTimer::timeout, this, [=] () {
        emit timeElapsed(_elapsed.elapsed());
    });
}

qint64 ElapsedTimer::elapsed()
{
    if (_elapsed.isValid()) {
        return _elapsed.elapsed();
    }
    return finalTime;
}

bool ElapsedTimer::isRunning() {
    return _elapsed.isValid();
}

void ElapsedTimer::start(int updateRate) {
    _interval.start(updateRate);
    _elapsed.start();
}

void ElapsedTimer::stop() {
    _interval.stop();
    finalTime = _elapsed.elapsed();
    _elapsed.invalidate();
}
