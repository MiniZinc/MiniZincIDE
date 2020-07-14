#ifndef PROCESS_H
#define PROCESS_H

#include <QProcess>
#include <QTextStream>
#include <QVersionNumber>
#include <QElapsedTimer>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

///
/// \brief The Process class
/// Extends QProcess with the ability to handle child processes.
///
class MznProcess : public QProcess {
#ifdef Q_OS_WIN
    Q_OBJECT
#endif
public:
    MznProcess(QObject* parent=nullptr) : QProcess(parent) {}
    void start(const QString& program, const QStringList& arguments, const QString& path);
    void terminate(void);
protected:
    virtual void setupChildProcess();
#ifdef Q_OS_WIN
private:
    HANDLE jobObject;
private slots:
    void attachJob();
#endif
};

#endif // PROCESS_H
