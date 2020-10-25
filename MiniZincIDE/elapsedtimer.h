#ifndef ELAPSEDTIMER_H
#define ELAPSEDTIMER_H

#include <QObject>

#include <QTimer>
#include <QElapsedTimer>

class ElapsedTimer: public QObject
{
    Q_OBJECT
public:
    ElapsedTimer(QObject* parent = nullptr);

    qint64 elapsed();
    bool isRunning();

signals:
    void timeElapsed(qint64 time);

public slots:
    void start(int updateRate);
    void stop();

private:
    QTimer _interval;
    QElapsedTimer _elapsed;
    qint64 finalTime;
};

#endif // ELAPSEDTIMER_H
