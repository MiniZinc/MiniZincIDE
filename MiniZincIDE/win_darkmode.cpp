#include "win_darkmode.h"

DarkModeNotifier::DarkModeNotifier(QObject* parent) : QObject(parent)
{
    auto result = RegOpenKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", &_hKey);
    _ok = result == ERROR_SUCCESS;
    _darkMode = retrieveDarkMode();
    if (!_ok) { return; }
    _hEvent = CreateEvent(nullptr, true, false, nullptr);
    _ok = _hEvent != nullptr;
    if (!_ok) { return; }
    _notifier = new QWinEventNotifier(_hEvent);
    connect(_notifier, &QWinEventNotifier::activated, this, &DarkModeNotifier::onNotifierActivate);
    _ok = result == ERROR_SUCCESS;
    if (!_ok) { return; }
    result = RegNotifyChangeKeyValue(_hKey,
                                     true,
                                     REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET,
                                     _hEvent,
                                     true);
    _ok = result == ERROR_SUCCESS;
}

DarkModeNotifier::~DarkModeNotifier()
{
    delete _notifier;
    CloseHandle(_hEvent);
    RegCloseKey(_hKey);
}

bool DarkModeNotifier::supportsDarkMode()
{
    return _ok;
}

bool DarkModeNotifier::retrieveDarkMode()
{
    if (!_ok) { return false; }
    DWORD value = 1;
    DWORD size = sizeof(DWORD);
    auto result = RegGetValue(_hKey, nullptr, L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (result != ERROR_SUCCESS) {
        _ok = false;
        return false;
    }
    return value == 0;
}

void DarkModeNotifier::onNotifierActivate()
{
    bool newDarkMode = retrieveDarkMode();
    if (!_ok) { return; }
    if (newDarkMode != _darkMode) {
        _darkMode = newDarkMode;
        emit darkModeChanged(newDarkMode);
    }
    _ok = ResetEvent(_hEvent) != 0;
    if (!_ok) { return; }
    auto result = RegNotifyChangeKeyValue(_hKey,
                                          true,
                                          REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET,
                                          _hEvent,
                                          true);
    _ok = result == ERROR_SUCCESS;
}
