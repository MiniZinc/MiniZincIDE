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

#include <QDebug>

#include "highlighter.h"


Highlighter::Highlighter(QFont& font, QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    Rule rule;

    QTextCharFormat format;

    format.setForeground(Qt::darkGreen);
    format.setFontWeight(QFont::Bold);

    quoteFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegExp("\".*\"");
    rule.pattern.setMinimal(true);
    rule.format = quoteFormat;
    rules.append(rule);

    commentFormat.setForeground(Qt::red);
    rule.pattern = QRegExp("%[^\n]*");
    rule.format = commentFormat;
    rules.append(rule);

    QStringList patterns;
    patterns << "\\bann\\b" << "\\bannotation\\b" << "\\bany\\b"
             << "\\barray\\b" << "\\bbool\\b" << "\\bcase\\b"
             << "\\bconstraint\\b" << "\\bdefault\\b" << "\\bdiv\\b"
             << "\\bdiff\\b" << "\\belse\\b" << "\\belseif\\b"
             << "\\bendif\\b" << "\\benum\\b" << "\\bfloat\\b"
             << "\\bfunction\\b" << "\\bif\\b" << "\\binclude\\b"
             << "\\bintersect\\b" << "\\bin\\b" << "\\bint\\b"
             << "\\blet\\b" << "\\bmaximize\\b" << "\\bminimize\\b"
             << "\\bmod\\b" << "\\bnot\\b" << "\\bof\\b" << "\\boutput\\b"
             << "\\bopt\\b" << "\\bpar\\b" << "\\bpredicate\\b"
             << "\\brecord\\b" << "\\bsatisfy\\b" << "\\bset\\b"
             << "\\bsolve\\b" << "\\bstring\\b" << "\\bsubset\\b"
             << "\\bsuperset\\b" << "\\bsymdiff\\b" << "\\btest\\b"
             << "\\bthen\\b" << "\\btuple\\b" << "\\btype\\b"
             << "\\bunion\\b" << "\\bvar\\b" << "\\bvariant_record\\b"
             << "\\bwhere\\b" << "\\bxor\\b";
    for (int i=0; i<patterns.size(); i++) {
        rule.pattern = QRegExp(patterns[i]);
        rule.format = format;
        rules.append(rule);
    }

    format = QTextCharFormat();
    format.setFontItalic(true);
    format.setForeground(Qt::blue);
    rule.pattern = QRegExp("\\b[A-Za-z0-9_]+(?=\\s*\\()");
    rule.format = format;
    rules.append(rule);

    setEditorFont(font);

    commentStartExp = QRegExp("/\\*");
    commentEndExp = QRegExp("\\*/");

}

void Highlighter::setEditorFont(QFont& font)
{
    quoteFormat.setFont(font);
    commentFormat.setFont(font);
    for (int i=0; i<rules.size(); i++) {
        rules[i].format.setFont(font);
    }
}

void Highlighter::highlightBlock(const QString &text)
{
    for (int i=0; i<rules.size(); i++) {
        const Rule& rule = rules[i];
        QRegExp expression(rule.pattern);
        int index = expression.indexIn(text);
        while (index >= 0) {
            int length = expression.matchedLength();
            if (format(index)!=quoteFormat && format(index)!=commentFormat) {
                setFormat(index, length, rule.format);
            }
            index = expression.indexIn(text, index + length);
        }
    }

    BracketData* bd = new BracketData;
    QRegExp re("\\(|\\)|\\{|\\}|\\[|\\]");
    int pos = text.indexOf(re);
    while (pos != -1) {
        if (format(pos)!=quoteFormat && format(pos)!=commentFormat) {
            Bracket b;
            b.b = text.at(pos);
            b.pos = pos;
            bd->brackets.append(b);
        }
        pos = text.indexOf(re, pos+1);
    }
    setCurrentBlockUserData(bd);
    setCurrentBlockState(0);

    int commentStartIndex = 0;
    if (previousBlockState() != 1) {
        commentStartIndex = commentStartExp.indexIn(text);
    }
    while (commentStartIndex >= 0) {
        int commentEndIndex = commentEndExp.indexIn(text, commentStartIndex);
        int commentLength;
        if (commentEndIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - commentStartIndex;
        } else {
            commentLength = commentEndIndex - commentStartIndex + commentEndExp.matchedLength();
        }
        setFormat(commentStartIndex, commentLength, commentFormat);
        commentStartIndex = commentStartExp.indexIn(text, commentStartIndex + commentLength);
    }
}
