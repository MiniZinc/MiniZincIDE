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

#include "theme.h"
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QRegularExpression>

struct Bracket {
    QChar b;
    int pos;
};

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
typedef QHash<FixedBg, QPair<QColor, QString> > BgMap;
uint qHash(const FixedBg& key);

inline bool operator==(const FixedBg &A, const FixedBg &B) {
    return A.sl == B.sl && A.sc == B.sc && A.el == B.el && A.ec == B.ec;
}

class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QFont& font, const Theme& theme, bool darkMode, QTextDocument *parent = 0);
    void setEditorFont(QFont& font);
    void copyHighlightedToClipboard(QTextCursor selectionCursor);
    void setTheme(const Theme& theme, bool darkMode);
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

};

#endif // HIGHLIGHTER_H
