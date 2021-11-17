#ifndef PROFILE_COMPILATION_H
#define PROFILE_COMPILATION_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>

#include "highlighter.h"

class Path
{
public:
    struct Segment
    {
        QString filename;
        int firstLine;
        int firstColumn;
        int lastLine;
        int lastColumn;
        QStringList parts;
    };

    Path(const QString& path);
    Path() {}

    const QVector<Segment>& segments() const { return _segments; }

private:
    QVector<Segment> _segments;
};

class PathEntry
{
public:
    PathEntry(const QJsonObject& obj);
    PathEntry() {}

    const QString& flatZincName() const {return _flatZincName; }
    const QString& niceName() const { return _niceName; }
    int constraintIndex() const { return _constraintIndex; }
    const Path& path() const { return _path; }
private:
    QString _flatZincName;
    QString _niceName;
    int _constraintIndex = -1;
    Path _path;
};

class TimingEntry
{
public:
    TimingEntry(const QJsonObject& obj);
    TimingEntry() {}

    const QString& filename() const { return _filename; }
    int line() const { return _line; }
    int time() const { return _time; }

private:
    QString _filename;
    int _line;
    int _time;
};

#endif // PROFILE_COMPILATION_H
