#ifndef THEME_H
#define THEME_H

#include <QObject>
#include <QColor>
#include <vector>

struct ThemeColor
{
    QColor light;
    QColor dark;

    ThemeColor(QColor lightAndDark) {
        light = lightAndDark;
        dark = lightAndDark;
    }

    ThemeColor(QColor l, QColor d) {
        light = l;
        dark = d;
    }

    QColor get(bool darkMode) const {
        return darkMode ? dark : light;
    }
};

struct Theme
{
    QString name;

    ThemeColor textColor, // Color of normal text
        keywordColor, // Color of MiniZinc keywords such as "in" "where" "constraints"
        functionColor, // Color of words that are followed by a function call, such as "forall()"
        stringColor, // Color of strings
        commentColor, // Color of comments %
        backgroundColor, // Color of background
        errorColor, //Color of error messages
        warningColor, // Color of warning messages
        lineHighlightColor,  // Color of highlighted line
        foregroundActiveColor, // color of active line number
        foregroundInactiveColor, // color of inactive line number
        textHighlightColor,      // color of highlighted text
        bracketsMatchColor, // Color of matching brackets
        bracketsNoMatchColor, //Colors of dangling brackets
        lineNumberbackground // Color of line numbers background
        ;

    bool isSystemTheme = false;

    Theme(const QString& _name,
        ThemeColor _textColor,
          ThemeColor _keywordColor,
          ThemeColor _functionColor,
          ThemeColor _stringColor,
          ThemeColor _commentColor,
          ThemeColor _backgroundColor,
          ThemeColor _lineHighlightColor,
          ThemeColor _textHighlightColor,
          ThemeColor _errorColor,
          ThemeColor _warningColor,
          bool systemTheme = false) :
        name(_name),
        textColor(_textColor),
        keywordColor(_keywordColor),
        functionColor(_functionColor),
        stringColor( _stringColor),
        commentColor(_commentColor),
        backgroundColor(_backgroundColor),
        errorColor(_errorColor),
        warningColor(_warningColor),
        lineHighlightColor(_lineHighlightColor),
        foregroundActiveColor(textColor),
        foregroundInactiveColor(textColor.light.lighter(), textColor.dark.darker()),
        textHighlightColor(_textHighlightColor),
        bracketsMatchColor(keywordColor.light.lighter(200), keywordColor.dark.darker(150)),
        bracketsNoMatchColor(errorColor.light, errorColor.dark),
        lineNumberbackground(backgroundColor.light.darker(110), backgroundColor.dark.lighter(150)),
        isSystemTheme(systemTheme) {}

    QString styleSheet(bool darkMode) const;
};


class ThemeManager : public QObject
{
    Q_OBJECT
public:
    explicit ThemeManager(QObject *parent = nullptr);

    const std::vector<Theme>& themes() const
    {
        return _themes;
    }
    const Theme& current() const
    {
        return _themes[_current];
    }
    void current(size_t index)
    {
        if (index < 0 || index >= _themes.size()) {
            _current = 0;
        } else {
            _current = index;
        }
    }
    const Theme& get(size_t index) const
    {
        if (index < 0 || index >= _themes.size()) {
            return _themes[0];
        }
        return _themes[index];
    }
private:
    size_t _current = 0;
    std::vector<Theme> _themes;
};

#endif // THEME_H
