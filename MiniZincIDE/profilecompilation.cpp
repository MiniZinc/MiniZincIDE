#include "profilecompilation.h"
#include <QFileInfo>

Path::Path(const QString& path)
{
    auto items = path.split(';');
    for (auto& it : items) {
        auto parts = it.split('|');
        if (parts.size() < 5) {
            continue;
        }
        Path::Segment segment;
        QFileInfo fi(parts[0]);
        segment.filename = fi.canonicalFilePath();
        segment.firstLine = parts[1].toInt();
        segment.firstColumn = parts[2].toInt();
        segment.lastLine = parts[3].toInt();
        segment.lastColumn = parts[4].toInt();
        QStringList rest;
        for (auto i = 5; i < parts.size(); i++) {
            segment.parts << parts[i];
        }
        _segments << segment;
    }
}

PathEntry::PathEntry(const QJsonObject& obj) : _path(obj["path"].toString())
{
    if (obj["constraintIndex"].isUndefined()) {
        _flatZincName = obj["flatZincName"].toString();
        _niceName = obj["niceName"].toString();
    } else {
        _constraintIndex = obj["constraintIndex"].toInt();
    }
}

TimingEntry::TimingEntry(const QJsonObject& obj)
{
    QFileInfo fi(obj["filename"].toString());
    _filename = fi.canonicalFilePath();
    _line = obj["line"].toInt();
    _time = obj["time"].toInt();
}
