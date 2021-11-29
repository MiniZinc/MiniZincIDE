/*
 *  Author:
 *     Guido Tack <guido.tack@monash.edu>
 *
 *  Copyright:
 *     NICTA 2013
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QRegularExpression>

struct Bracket {
    QChar b;
    int pos;
};

class ThemeColor{
public:
    QColor light;
    QColor dark;
    ThemeColor(){
        light = Qt::red;
        dark = Qt::red;
    }

    ThemeColor(QColor lightAndDark){
        light = lightAndDark;
        dark = lightAndDark;
    }

    ThemeColor(QColor l, QColor d){
        light = l;
        dark = d;
    }
    QColor get(bool darkMode){
        return darkMode?dark:light;
    }
};

struct Theme
{
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
        Theme(): Theme(Qt::white,Qt::darkGreen, Qt::blue, Qt::darkRed, Qt::red, Qt::white, Qt::lightGray, Qt::gray) {}

        Theme (QColor _textColor,
               QColor _keywordColor,
               QColor _functionColor,
               QColor _stringColor,
               QColor _commentColor,
               QColor _backgroundColor,
               QColor _lineHighlightColor,
               QColor _textHighlightColor,
               bool systemTheme = false):
            Theme(ThemeColor(_textColor,_textColor),
            ThemeColor(_keywordColor,_keywordColor),
            ThemeColor(_functionColor,_functionColor),
            ThemeColor( _stringColor,_stringColor),
            ThemeColor(_commentColor,_commentColor),
            ThemeColor(_backgroundColor,_backgroundColor),
            ThemeColor(_lineHighlightColor,_lineHighlightColor),
            ThemeColor(_textHighlightColor,_textHighlightColor),
            ThemeColor(Qt::red,Qt::red),
            ThemeColor(Qt::yellow, Qt::yellow),
            systemTheme)
            {}

        Theme(ThemeColor _textColor,
              ThemeColor _keywordColor,
              ThemeColor _functionColor,
              ThemeColor _stringColor,
              ThemeColor _commentColor,
              ThemeColor _backgroundColor,
              ThemeColor _lineHighlightColor,
              ThemeColor _textHighlightColor,
              ThemeColor _errorColor,
              ThemeColor _warningColor,
              bool systemTheme = false):
            textColor(_textColor),
            keywordColor(_keywordColor),
            functionColor(_functionColor),
            stringColor( _stringColor),
            commentColor(_commentColor),
            backgroundColor(_backgroundColor),
            errorColor(_errorColor),
            warningColor(_warningColor),
            lineHighlightColor(_lineHighlightColor),
            textHighlightColor(_textHighlightColor),
            isSystemTheme(systemTheme) {

            bracketsMatchColor = ThemeColor(keywordColor.light.lighter(200), keywordColor.dark.darker(150));
            bracketsNoMatchColor = ThemeColor(errorColor.light, errorColor.dark);//QColor(0xC24223)

            foregroundActiveColor = textColor;
            foregroundInactiveColor = ThemeColor(textColor.light.lighter(), textColor.dark.darker());

            lineNumberbackground = ThemeColor(backgroundColor.light.darker(110), backgroundColor.dark.lighter(150));
        }

        QString styleSheet(bool darkMode) {
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
};

namespace Themes{
    static Theme blueberry = Theme(ThemeColor(QColor(0x0B1224), QColor(0x8EBBED)), // text
                                   ThemeColor(QColor(0x3354A7), QColor(0x1172A6)), //keywords
                                   ThemeColor(QColor(0x9147A6), QColor(0xD676CC)), // function
                                   ThemeColor(QColor(0x132F6B), QColor(0xA192E8)), // string
                                   ThemeColor(QColor(0x83B9F5), QColor(0x225773)), //comment
                                   ThemeColor(QColor(0xE9ECFF),QColor(0x001926)), //background
                                   ThemeColor(QColor(0xE7E3FF),QColor(0x001D2B)), // line highlight
                                   ThemeColor(QColor(0xBEBDFC),QColor(0x005580)), // text highlight
                                   ThemeColor(Qt::red), // error color
                                   ThemeColor(Qt::yellow) // warning color
                                   );

    static Theme mango = Theme(ThemeColor(QColor(0x375327), QColor(0xF2DC6D)), // text
                               ThemeColor(QColor(0xF27B35), QColor(0xF2385A)), //keywords
                               ThemeColor(QColor(0xF2385A), QColor(0xF29F05)), // function
                               ThemeColor(QColor(0x51C0BF), QColor(0xF48985)), // string
                               ThemeColor(QColor(0x6A8F2F), QColor(0xF4503F)), //comment
                               ThemeColor(QColor(0xEFDC9B),QColor(0x030500)), //background
                               ThemeColor(QColor(0xD9C78D),QColor(0x070D00)), // line highlight
                               ThemeColor(QColor(0xCFB359),QColor(0x6B6033)), // text highlight
                               ThemeColor(Qt::red, QColor(0x51C0BF)), // error color
                               ThemeColor(Qt::yellow) // warning color
                               );

    static Theme minizinc = Theme(ThemeColor(Qt::black, Qt::white), // text
                                  ThemeColor(Qt::darkGreen, QColor(0xbb86fc)), //keywords
                                  ThemeColor(Qt::blue, QColor(0x13C4F5)), // function
                                  ThemeColor(Qt::darkRed, QColor(0xF29F05)), // string
                                  ThemeColor(Qt::red, QColor(0x52514C)), //comment
                                  ThemeColor(Qt::white,QColor(0x181820)), //background
                                  ThemeColor(QColor(0xF0F0F0),QColor(0x1D1D26)), // line highlight
                                  ThemeColor(QColor(0xE7E3FF),QColor(0x001D2B)), // text highlight
                                  ThemeColor(Qt::red), // error color
                                  ThemeColor(QColor(0xd1ab13), Qt::yellow), // warning color
                                  true
                                  );
    extern Theme currentTheme;
}

class DebugInfoData {
public:
    int con; // DebugInfo: number of constraints
    int var; // DebugInfo: number of variables
    int ms;  // DebugInfo: milliseconds
    int totalCon;
    int totalVar;
    int totalMs;
    DebugInfoData(void) : con(0), var(0), ms(0), totalCon(0), totalVar(0), totalMs(1) {}
    bool hasData(void) { return con!=0 || var!=0 || ms!=0; }
    void reset(void) { con=0; var=0; ms=0; totalCon=0; totalVar=0; totalMs=1; }
    QString toString(void) {
        return QString().number(ms)+"ms,"+QString().number(con)+","+QString().number(var);
    }
};

enum HighlightingState { HS_NONE=-1, HS_STRING, HS_INTERPOLATE, HS_COMMENT };

class BracketData : public QTextBlockUserData
{
public:
    QVector<Bracket> brackets;
    DebugInfoData d;
    QVector<HighlightingState> highlightingState;
};

struct FixedBg {
  unsigned int sl;
  unsigned int sc;
  unsigned int el;
  unsigned int ec;
};
typedef QMap<FixedBg, QPair<QColor, QString> > BgMap;
inline bool operator<(const FixedBg& A, const FixedBg& B) {
  return A.sl < B.sl || A.sc < B.sc || A.el < B.el || A.ec < B.ec;
}

class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QFont& font, bool darkMode, QTextDocument *parent = 0);
    void setEditorFont(QFont& font);
    void copyHighlightedToClipboard(QTextCursor selectionCursor);
    void setDarkMode(bool);
    void addFixedBg(unsigned int sl, unsigned int sc, unsigned int el, unsigned ec, QColor colour, QString tip);
    void clearFixedBg();
protected:
    void highlightBlock(const QString &text);

private:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<Rule> rules;

    BgMap fixedBg;

    QColor keywordColor;
    QColor functionColor;
    QColor stringColor;
    QColor commentColor;

    QTextCharFormat commentFormat;
    QRegularExpression commentStartExp;
    QRegularExpression commentEndExp;
    bool darkMode;

};

#endif // HIGHLIGHTER_H
