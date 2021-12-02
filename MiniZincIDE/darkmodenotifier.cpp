#include "darkmodenotifier.h"

#include <QSettings>

DarkModeNotifier::DarkModeNotifier(QObject *parent) : QObject(parent)
{
    init();

    if (!hasSystemSetting()) {
        QSettings settings;
        settings.beginGroup("MainWindow");
        _darkMode = settings.value("darkMode", false).value<bool>();
    }
}

void DarkModeNotifier::requestChangeDarkMode(bool enable)
{
    if (hasSystemSetting()) {
        // Can't change dark mode since we're using system setting
        return;
    }
    if (enable != _darkMode) {
        _darkMode = enable;
        emit darkModeChanged(_darkMode);
    }
}

#ifdef Q_OS_WIN

#include <QWinEventNotifier>

#define NOMINMAX
#include <Windows.h>

class DarkModeNotifier::Internal
{
public:
    QWinEventNotifier* notifier = nullptr;

    HKEY hKey = nullptr;
    HANDLE hEvent = nullptr;

    bool ok = true;
};

void DarkModeNotifier::init()
{
    auto getDarkMode = [=] () {
        if (!_internal->ok) { return false; }
        DWORD value = 1;
        DWORD size = sizeof(DWORD);
        auto result = RegGetValue(_internal->hKey, nullptr, L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
        if (result != ERROR_SUCCESS) {
            _internal->ok = false;
            return false;
        }
        return value == 0;
    };

    _internal = new Internal;

    // Get dark mode registry key
    auto result = RegOpenKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", &_internal->hKey);
    _internal->ok = result == ERROR_SUCCESS;
    if (!_internal->ok) { return; }

    // Get initial dark mode
    _darkMode = getDarkMode();
    if (!_internal->ok) { return; }

    // Setup event to be triggered on registry change
    _internal->hEvent = CreateEvent(nullptr, true, false, nullptr);
    _internal->ok = _internal->hEvent != nullptr;
    if (!_internal->ok) { return; }

    // Listen for event
    _internal->notifier = new QWinEventNotifier(_internal->hEvent);
    connect(_internal->notifier, &QWinEventNotifier::activated, this, [=] () {
        bool newDarkMode = getDarkMode();
        if (!_internal->ok) { return; }
        if (newDarkMode != _darkMode) {
            _darkMode = newDarkMode;
            emit darkModeChanged(newDarkMode);
        }
        _internal->ok = ResetEvent(_internal->hEvent) != 0;
        if (!_internal->ok) { return; }
        auto result = RegNotifyChangeKeyValue(_internal->hKey,
                                              true,
                                              REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET,
                                              _internal->hEvent,
                                              true);
        _internal->ok = result == ERROR_SUCCESS;
    });

    // Start notifier
    result = RegNotifyChangeKeyValue(_internal->hKey,
                                     true,
                                     REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET,
                                     _internal->hEvent,
                                     true);
    _internal->ok = result == ERROR_SUCCESS;
}

DarkModeNotifier::~DarkModeNotifier()
{
    delete _internal->notifier;
    CloseHandle(_internal->hEvent);
    RegCloseKey(_internal->hKey);
    delete _internal;
}

bool DarkModeNotifier::hasSystemSetting() const
{
    return _internal->ok;
}

bool DarkModeNotifier::hasNativeDarkMode() const
{
    // Have to style using CSS, no native dark widgets
    return false;
}

#elif !defined(Q_OS_MAC)

// No support for system dark mode

void DarkModeNotifier::init() {}

DarkModeNotifier::~DarkModeNotifier() {}

bool DarkModeNotifier::hasSystemSetting() const
{
    return false;
}

bool DarkModeNotifier::hasNativeDarkMode() const
{
    // Have to style using CSS, no native dark widgets
    return false;
}

#endif
