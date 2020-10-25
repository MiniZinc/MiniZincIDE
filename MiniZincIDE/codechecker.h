#ifndef CHECKCODE_H
#define CHECKCODE_H

#include <QObject>

#include "codeeditor.h"
#include "process.h"

class CodeChecker : public QObject
{
    Q_OBJECT
public:
    explicit CodeChecker(QObject *parent = nullptr);
    ~CodeChecker();

    void start(const QString& modelContents, SolverConfiguration& sc, const QString& wd);
    void cancel(void);
signals:
    void finished(const QVector<MiniZincError>& mznErrors);

private slots:
    void onStarted(void);
    void onLine(const QString& data);
    void onFinished();

private:
    MznProcess p;
    QString input;

    bool inRelevantError = false;
    MiniZincError curError;
    QVector<MiniZincError> mznErrors;
};

#endif // CHECKCODE_H
