/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QDebug>

#include "highlighter.h"


Highlighter::Highlighter(QFont& font, QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    keywordFormat.setForeground(Qt::darkGreen);
    keywordFormat.setFontWeight(QFont::Bold);
    QStringList keywordPatterns;

    keywordPatterns << "\\bann\\b" << "\\bannotation\\b" << "\\bany\\b"
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
    foreach (const QString &pattern, keywordPatterns) {
        rule.pattern = QRegExp(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    singleLineCommentFormat.setForeground(Qt::red);
    rule.pattern = QRegExp("%[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    quotationFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegExp("\".*\"");
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    functionFormat.setFontItalic(true);
    functionFormat.setForeground(Qt::blue);
    rule.pattern = QRegExp("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);
    setEditorFont(font);
}

void Highlighter::setEditorFont(QFont& font)
{
    baseFormat.setFont(font);
    for (int i=0; i<highlightingRules.size(); i++) {
        highlightingRules[i].format.setFont(font);
    }
}

void Highlighter::highlightBlock(const QString &text)
{
    setFormat(0, text.length(), baseFormat);
    foreach (const HighlightingRule &rule, highlightingRules) {
        QRegExp expression(rule.pattern);
        int index = expression.indexIn(text);
        while (index >= 0) {
            int length = expression.matchedLength();
            setFormat(index, length, rule.format);
            index = expression.indexIn(text, index + length);
        }
    }

    BracketData* bd = new BracketData;
    QRegExp re("\\(|\\)|\\{|\\}|\\[|\\]");
    int pos = text.indexOf(re);
    while (pos != -1) {
        if (format(pos)!=quotationFormat && format(pos)!=singleLineCommentFormat) {
            Bracket b;
            b.b = text.at(pos);
            b.pos = pos;
            bd->brackets.append(b);
        }
        pos = text.indexOf(re, pos+1);
    }
    setCurrentBlockUserData(bd);
    setCurrentBlockState(0);
}
