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


Highlighter::Highlighter(QFont& font, bool dm, QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    Rule rule;

    quoteFormat.setForeground(Qt::darkRed);
    rule.pattern = QRegExp("\"([^\"\\\\]|\\\\.)*\"");
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
    QTextCharFormat format;

    format.setFontWeight(QFont::Bold);
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

    setDarkMode(dm);

}

void Highlighter::setEditorFont(QFont& font)
{
    quoteFormat.setFont(font);
    commentFormat.setFont(font);
    for (int i=0; i<rules.size(); i++) {
        rules[i].format.setFont(font);
    }
}

bool fg_contains(const FixedBg& a, const FixedBg& b) {
  return (a.sl < b.sl || (a.sl == b.sl && a.sc <= b.sc))
      && (a.el > b.el || (a.el == b.el && a.ec >= b.ec));
}

void Highlighter::addFixedBg(
    unsigned int sl, unsigned int sc, unsigned int el, unsigned ec,
    QColor colour, QString tip) {

  FixedBg ifb {sl, sc, el, ec};

  for(BgMap::iterator it = fixedBg.begin();
      it != fixedBg.end();) {
    const FixedBg& fb = it.key();

    if (fg_contains(fb, ifb)) {
      it = fixedBg.erase(it);
    } else {
      ++it;
    }
  }
  fixedBg.insert(ifb, QPair<QColor, QString>(colour, tip));

}

void Highlighter::clearFixedBg() {
    fixedBg.clear();
}

void Highlighter::highlightBlock(const QString &text)
{
    QTextBlock block = currentBlock();
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

    for(QMap<FixedBg, QPair<QColor, QString> >::iterator it = fixedBg.begin();
        it != fixedBg.end(); ++it) {
      const FixedBg& fb = it.key();
      QPair<QColor, QString> val = it.value();
      QColor colour = val.first;
      QString tip = val.second;

      unsigned int blockNumber = block.blockNumber() + 1;
      if(fb.sl <= blockNumber && fb.el >= blockNumber) {
        int index = 0;
        int length = block.length();

        if(fb.sl == blockNumber) {
          index = fb.sc - 1;
          if(fb.sl == fb.el) {
            length = fb.ec - index;
          }
        } else if(fb.el == blockNumber) {
          length = fb.ec;
        }

        int endpos = index + length;
        foreach(const QTextLayout::FormatRange& fr, block.textFormats()) {
          //if(index >= fr.start && index <= fr.start + length) {
            int local_index = fr.start < index ? index : fr.start;
            int fr_endpos = fr.start + fr.length;
            int local_len = (fr_endpos < endpos ? fr_endpos : endpos) - local_index;
            QTextCharFormat fmt = fr.format;
            fmt.setBackground(colour);
            fmt.setToolTip(tip);
            setFormat(local_index, local_len, fmt);
          //}
        }
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

#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextLayout>
#include <QTextEdit>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

class MyMimeDataExporter : public QTextEdit {
public:
    QMimeData* md(void) const {
        QMimeData* mymd = createMimeDataFromSelection();
        mymd->removeFormat("text/plain");
        return mymd;
    }
};

void Highlighter::copyHighlightedToClipboard(QTextCursor cursor)
{
    QTextDocument* tempDocument(new QTextDocument);
    Q_ASSERT(tempDocument);
    QTextCursor tempCursor(tempDocument);

    tempCursor.insertFragment(cursor.selection());
    tempCursor.select(QTextCursor::Document);

    QTextCharFormat textfmt = cursor.charFormat();
    textfmt.setFont(quoteFormat.font());
    tempCursor.setCharFormat(textfmt);

    QTextBlock start = document()->findBlock(cursor.selectionStart());
    QTextBlock end = document()->findBlock(cursor.selectionEnd());
    end = end.next();
    const int selectionStart = cursor.selectionStart();
    const int endOfDocument = tempDocument->characterCount() - 1;
    for(QTextBlock current = start; current.isValid() && current != end; current = current.next()) {
        const QTextLayout* layout(current.layout());

        foreach(const QTextLayout::FormatRange &range, layout->additionalFormats()) {
            const int start = current.position() + range.start - selectionStart;
            const int end = start + range.length;
            if(end <= 0 || start >= endOfDocument)
                continue;
            tempCursor.setPosition(qMax(start, 0));
            tempCursor.setPosition(qMin(end, endOfDocument), QTextCursor::KeepAnchor);
            tempCursor.setCharFormat(range.format);
        }
    }

    for(QTextBlock block = tempDocument->begin(); block.isValid(); block = block.next())
        block.setUserState(-1);

    tempCursor.select(QTextCursor::Document);
    QTextBlockFormat blockFormat = cursor.blockFormat();
    blockFormat.setNonBreakableLines(true);
    tempCursor.setBlockFormat(blockFormat);

    MyMimeDataExporter te;
    te.setDocument(tempDocument);
    te.selectAll();
    QMimeData* mimeData = te.md();
    QApplication::clipboard()->setMimeData(mimeData);
    delete tempDocument;
}

void Highlighter::setDarkMode(bool enable)
{
    darkMode = enable;
    if (darkMode) {
        rules[0].format.setForeground(QColor(143,157,106));
        commentFormat.setForeground(QColor(90,90,90));
        rules[1].format.setForeground(QColor(90,90,90));
        for (int i=2; i<rules.size()-1; i++) {
            rules[i].format.setForeground(QColor(218,208,133));
        }
        rules[rules.size()-1].format.setForeground(QColor(155,112,63));
    } else {
        rules[0].format.setForeground(Qt::darkRed);
        rules[1].format.setForeground(Qt::red);
        commentFormat.setForeground(Qt::red);
        for (int i=2; i<rules.size()-1; i++) {
            rules[i].format.setForeground(Qt::darkGreen);
        }
        rules[rules.size()-1].format.setForeground(Qt::blue);
    }
}
