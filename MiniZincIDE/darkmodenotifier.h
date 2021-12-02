#ifndef DARKMODENOTIFIER_H
#define DARKMODENOTIFIER_H

#include <QObject>

class DarkModeNotifier : public QObject
{
    Q_OBJECT
public:
    explicit DarkModeNotifier(QObject *parent = nullptr);
    ~DarkModeNotifier();
    bool hasSystemSetting() const;
    bool hasNativeDarkMode() const;
    bool darkMode() const { return _darkMode; }
public slots:
    void requestChangeDarkMode(bool enable);

signals:
    void darkModeChanged(bool darkMode);
private:
    void init();
    class Internal;
    Internal* _internal = nullptr;
    bool _darkMode = false;
};

#endif // DARKMODENOTIFIER_H
