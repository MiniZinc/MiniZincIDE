#ifndef WIN_DARKMODE_H
#define WIN_DARKMODE_H

#include <QObject>
#include <QWinEventNotifier>

#define NOMINMAX
#include <Windows.h>

class DarkModeNotifier: public QObject
{
    Q_OBJECT
public:
    explicit DarkModeNotifier(QObject* parent = nullptr);
    ~DarkModeNotifier();

    bool supportsDarkMode();
    bool darkMode() { return _darkMode; }
signals:
    void darkModeChanged(bool darkMode);
private:
    QWinEventNotifier* _notifier = nullptr;

    HKEY _hKey = nullptr;
    HANDLE _hEvent = nullptr;

    bool _ok = true;
    bool _darkMode = false;

    bool retrieveDarkMode();
private slots:
    void onNotifierActivate();
};

#endif // WIN_DARKMODE_H
