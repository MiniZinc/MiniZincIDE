#include "theme.h"

QString Theme::styleSheet(bool darkMode) const
{
    auto style_sheet = QString("background-color: %1;"
                               "color: %2;")
                           .arg(backgroundColor.get(darkMode).name(QColor::HexArgb))
                           .arg(textColor.get(darkMode).name(QColor::HexArgb));

    if (!isSystemTheme) {
        // Only change highlight colour for non-system themes
        // Many platforms have settings/accessibility options for this, so we should probably follow it by default
        style_sheet += QString(
                           "selection-background-color: %1;"
                           "selection-color: %2;"
                           )
                           .arg(textHighlightColor.get(darkMode).name(QColor::HexArgb))
                           .arg(textColor.get(darkMode).name(QColor::HexArgb));
    }

    return style_sheet;
}



ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    _themes.push_back({"Default",
                       ThemeColor(Qt::black, Qt::white), // text
                       ThemeColor(Qt::darkGreen, QColor(0xbb86fc)), //keywords
                       ThemeColor(Qt::blue, QColor(0x13C4F5)), // function
                       ThemeColor(Qt::darkRed, QColor(0xF29F05)), // string
                       ThemeColor(Qt::red, QColor(0x616161)), //comment
                       ThemeColor(Qt::white,QColor(0x222222)), //background
                       ThemeColor(QColor(0xF0F0F0),QColor(0x292929)), // line highlight
                       ThemeColor(QColor(0xE7E3FF),QColor(0x004161)), // text highlight
                       ThemeColor(Qt::red), // error color
                       ThemeColor(QColor(0xd1ab13), Qt::yellow), // warning color
                       true
    });
    _themes.push_back({"Blueberry",
                       ThemeColor(QColor(0x0B1224), QColor(0x8EBBED)), // text
                       ThemeColor(QColor(0x3354A7), QColor(0x1172A6)), //keywords
                       ThemeColor(QColor(0x9147A6), QColor(0xD676CC)), // function
                       ThemeColor(QColor(0x132F6B), QColor(0xA192E8)), // string
                       ThemeColor(QColor(0x83B9F5), QColor(0x225773)), //comment
                       ThemeColor(QColor(0xE9ECFF),QColor(0x001926)), //background
                       ThemeColor(QColor(0xE7E3FF),QColor(0x001D2B)), // line highlight
                       ThemeColor(QColor(0xBEBDFC),QColor(0x005580)), // text highlight
                       ThemeColor(Qt::red), // error color
                       ThemeColor(Qt::yellow) // warning color
    });
    _themes.push_back({"Mango",
                       ThemeColor(QColor(0x375327), QColor(0xF2DC6D)), // text
                       ThemeColor(QColor(0xF27B35), QColor(0xF2385A)), //keywords
                       ThemeColor(QColor(0xF2385A), QColor(0xF29F05)), // function
                       ThemeColor(QColor(0x51C0BF), QColor(0xF48985)), // string
                       ThemeColor(QColor(0x6A8F2F), QColor(0xF4503F)), //comment
                       ThemeColor(QColor(0xEFDC9B),QColor(0x030500)), //background
                       ThemeColor(QColor(0xD9C78D),QColor(0x070D00)), // line highlight
                       ThemeColor(QColor(0xCFB359),QColor(0x6B6033)), // text highlight
                       ThemeColor(Qt::red, QColor(0x51C0BF)), // error color
                       ThemeColor(Qt::yellow) // warning color
    });
}
