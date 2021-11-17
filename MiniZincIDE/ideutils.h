#ifndef IDEUTILS_H
#define IDEUTILS_H

#include <QString>

namespace IDEUtils {
    QString formatTime(qint64 time);
    bool isChildPath(const QString& parent, const QString& child);
}

#endif // IDEUTILS_H
