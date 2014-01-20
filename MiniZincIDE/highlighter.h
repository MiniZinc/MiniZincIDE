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

struct Bracket {
    QChar b;
    int pos;
};

class BracketData : public QTextBlockUserData
{
public:
    QVector<Bracket> brackets;
};

class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QFont& font, QTextDocument *parent = 0);
    void setEditorFont(QFont& font);
protected:
    void highlightBlock(const QString &text);

private:
    struct Rule
    {
        QRegExp pattern;
        QTextCharFormat format;
    };
    QVector<Rule> rules;

    QTextCharFormat quoteFormat;
    QTextCharFormat commentFormat;
};

#endif // HIGHLIGHTER_H
