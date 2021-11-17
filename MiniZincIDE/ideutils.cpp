#include "ideutils.h"

#include <QDir>
#include <QFileInfo>

namespace IDEUtils {

QString formatTime(qint64 time) {
    int hours =  time / 3600000;
    int minutes = (time % 3600000) / 60000;
    int seconds = (time % 60000) / 1000;
    int msec = (time % 1000);
    QString elapsed;
    if (hours > 0) {
        elapsed += QString("%1h ").arg(hours);
    }
    if (hours > 0 || minutes > 0) {
        elapsed += QString("%1m ").arg(minutes);
    }
    if (hours > 0 || minutes > 0 || seconds > 0) {
        elapsed += QString("%1s").arg(seconds);
    }
    if (hours==0 && minutes==0) {
        if (seconds > 0) {
            elapsed += " ";
        }
        elapsed += QString("%1msec").arg(msec);
    }
    return elapsed;
}

bool isChildPath(const QString& parent, const QString& child)
{
    return !QDir(parent).relativeFilePath(child).startsWith(".");
}

}
