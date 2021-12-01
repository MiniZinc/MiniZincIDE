#ifndef IDEUTILS_H
#define IDEUTILS_H

#include <QString>
#include <QWidget>

namespace IDEUtils {
    QString formatTime(qint64 time);
    bool isChildPath(const QString& parent, const QString& child);
    void watchChildChanges(QWidget* target, QObject* receiver, std::function<void()> action);
    template <class T>
    void watchChildChanges(QWidget* target, T* receiver, void (T::* action)()) {
        watchChildChanges(target, receiver, std::bind(action, receiver));
    }
}

#endif // IDEUTILS_H
