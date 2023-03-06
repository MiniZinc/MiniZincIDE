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
#include "ideutils.h"

Theme Themes::currentTheme = Themes::minizinc;

Highlighter::Highlighter(QFont& font, bool dm, QTextDocument *parent)
    : QSyntaxHighlighter(parent), keywordColor(Qt::darkGreen), functionColor(Qt::blue), stringColor(Qt::darkRed), commentColor(Qt::red)

{
    Rule rule;

    commentFormat.setForeground(commentColor);

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

    format.setForeground(functionColor);
    rule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+(?=\\s*\\()");
    rule.format = format;
    rules.append(rule);

    format = QTextCharFormat();
    format.setFontWeight(QFont::Bold);
    format.setForeground(keywordColor);
    for (int i=0; i<patterns.size(); i++) {
        rule.pattern = QRegularExpression(patterns[i]);
        rule.format = format;
        rules.append(rule);
    }


    setEditorFont(font);

    commentStartExp = QRegularExpression("/\\*");
    commentEndExp = QRegularExpression("\\*/");

    setDarkMode(dm);

}

void Highlighter::setEditorFont(QFont& font)
{
    commentFormat.setFont(font);
    for (int i=0; i<rules.size(); i++) {
        rules[i].format.setFont(font);
    }
}

uint qHash(const FixedBg& key) {
    return qHash(key.sl) ^ qHash(key.sc) ^ qHash(key.el) ^ qHash(key.ec);
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

    BracketData* bd = new BracketData;
    if (currentBlockUserData()) {
        bd->d = static_cast<BracketData*>(currentBlockUserData())->d;
    }
    if (block.previous().userData()) {
        bd->highlightingState = static_cast<BracketData*>(block.previous().userData())->highlightingState;
    }

    QRegularExpression noneRegExp("\"|%|/\\*");
    QRegularExpression stringRegExp("\"|\\\\\\(");
    QRegularExpression commentRegexp("\\*/");
    QRegularExpression interpolateRegexp("[)\"%(]|/\\*");

    QTextCharFormat stringFormat;
    stringFormat.setForeground(stringColor);
    stringFormat.setFontItalic(true);
    QTextCharFormat interpolateFormat;
    interpolateFormat.setFontItalic(true);

    // Stage 1: find strings (including interpolations) and comments
    QVector<HighlightingState>& highlightingState = bd->highlightingState;
    int curPosition = 0;
    HighlightingState currentState;
    while (curPosition >= 0) {
        currentState = highlightingState.empty() ? HS_NONE : highlightingState.back();
        switch (currentState) {
        case HS_NONE:
            {
                int nxt = noneRegExp.match(text, curPosition).capturedStart();
                if (nxt==-1) {
                    curPosition = -1;
                } else {
                    if (text[nxt]=='"') {
                        highlightingState.push_back(HS_STRING);
                        curPosition = nxt+1;
                    } else if (text[nxt]=='%') {
                        setFormat(nxt, text.size()-nxt, commentFormat);
                        curPosition = -1;
                    } else {
                        // /*
                        highlightingState.push_back(HS_COMMENT);
                        curPosition = nxt+2;
                    }
                }
            }
            break;
        case HS_STRING:
            {
                int nxt = stringRegExp.match(text, curPosition).capturedStart();
                int stringStartIdx = curPosition==0 ? 0 : curPosition-1;
                if (nxt==-1) {
                    setFormat(stringStartIdx, text.size()-stringStartIdx, stringFormat);
                    highlightingState.clear(); // this is an error, reset to NONE state
                    curPosition = -1;
                } else {
                    if (text[nxt]=='"') {
                        setFormat(stringStartIdx, nxt-stringStartIdx+1, stringFormat);
                        curPosition = nxt+1;
                        highlightingState.pop_back();
                    } else {
                        setFormat(stringStartIdx, nxt-stringStartIdx+2, stringFormat);
                        curPosition = nxt+2;
                        highlightingState.push_back(HS_INTERPOLATE);
                    }
                }
            }
            break;
        case HS_COMMENT:
            {
                int nxt = commentRegexp.match(text, curPosition).capturedStart();
                int commentStartIdx = curPosition <= 1 ? 0 : curPosition - 2;
                if (nxt==-1) {
                    // EOL -> stay in COMMENT state
                    setFormat(commentStartIdx, text.size()-commentStartIdx, commentFormat);
                    curPosition = -1;
                } else {
                    // finish comment
                    setFormat(commentStartIdx, nxt-commentStartIdx+2, commentFormat);
                    curPosition = nxt+2;
                    highlightingState.pop_back();
                }
            }
            break;
        case HS_INTERPOLATE:
            {
                int nxt = interpolateRegexp.match(text, curPosition).capturedStart();
                if (nxt==-1) {
                    // EOL -> stay in INTERPOLATE state
                    setFormat(curPosition, text.size()-curPosition, interpolateFormat);
                    curPosition = -1;
                } else {
                    setFormat(curPosition, nxt-curPosition+1, interpolateFormat);
                    if (text[nxt]==')') {
                        curPosition = nxt+1;
                        highlightingState.pop_back();
                    } else if (text[nxt]=='(') {
                        curPosition = nxt+1;
                        highlightingState.push_back(HS_INTERPOLATE);
                    } else if (text[nxt]=='%') {
                        setFormat(nxt, text.size()-nxt, commentFormat);
                        curPosition = -1;
                    } else if (text[nxt]=='"') {
                        curPosition = nxt+1;
                        highlightingState.push_back(HS_STRING);
                    } else {
                        // /*
                        highlightingState.push_back(HS_COMMENT);
                        curPosition = nxt+1;
                    }
                }
            }
            break;
        }
    }
    currentState = highlightingState.empty() ? HS_NONE : highlightingState.back();
    setCurrentBlockState(currentState);

    // Stage 2: find keywords and functions
    for (int i=0; i<rules.size(); i++) {
        const Rule& rule = rules[i];
        QRegularExpression expression(rule.pattern);
        auto match = expression.match(text);
        int index = match.capturedStart();
        while (index >= 0) {
            int length = match.capturedLength();
            if (format(index)!=commentFormat && format(index)!=stringFormat) {
                if (format(index)==interpolateFormat) {
                    QTextCharFormat interpolateRule = rule.format;
                    interpolateRule.setFontItalic(true);
                    setFormat(index, length, interpolateRule);
                } else {
                    setFormat(index, length, rule.format);
                }
            }
            match = expression.match(text, index + length);
            index = match.capturedStart();
        }
    }

    // Stage 3: update bracket data
    QRegularExpression re("\\(|\\)|\\{|\\}|\\[|\\]");
    auto re_match = re.match(text);
    int pos = re_match.capturedStart();
    while (pos != -1) {
        if (format(pos)!=commentFormat) {
            Bracket b;
            b.b = text.at(pos);
            b.pos = pos;
            bd->brackets.append(b);
        }
        re_match = re.match(text, pos+1);
        pos = re_match.capturedStart();
    }
    setCurrentBlockUserData(bd);

    // Stage 4: apply highlighting
    for(QHash<FixedBg, QPair<QColor, QString> >::iterator it = fixedBg.begin();
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
}

#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextLayout>
#include <QTextEdit>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

void Highlighter::copyHighlightedToClipboard(QTextCursor cursor)
{
    QTextDocument* tempDocument(new QTextDocument);
    Q_ASSERT(tempDocument);
    QTextCursor tempCursor(tempDocument);

    tempCursor.insertFragment(cursor.selection());
    tempCursor.select(QTextCursor::Document);

    QTextCharFormat textfmt = cursor.charFormat();
    textfmt.setFont(commentFormat.font());
    tempCursor.setCharFormat(textfmt);

    QTextBlock start = document()->findBlock(cursor.selectionStart());
    QTextBlock end = document()->findBlock(cursor.selectionEnd());
    end = end.next();
    const int selectionStart = cursor.selectionStart();
    const int endOfDocument = tempDocument->characterCount() - 1;
    for(QTextBlock current = start; current.isValid() && current != end; current = current.next()) {
        const QTextLayout* layout(current.layout());

        foreach(const QTextLayout::FormatRange &range, layout->formats()) {
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

    IDEUtils::MimeDataExporter te;
    te.setDocument(tempDocument);
    te.selectAll();
    QMimeData* mimeData = te.md();
    QApplication::clipboard()->setMimeData(mimeData);
    delete tempDocument;
}

void Highlighter::setDarkMode(bool enable)
{
    darkMode = enable;
    keywordColor = Themes::currentTheme.keywordColor.get(darkMode);
    functionColor = Themes::currentTheme.functionColor.get(darkMode);
    stringColor = Themes::currentTheme.stringColor.get(darkMode);
    commentColor = Themes::currentTheme.commentColor.get(darkMode);
    commentFormat.setForeground(commentColor);
    rules[0].format.setForeground(functionColor);
    for (int i=1; i<rules.size(); i++) {
        rules[i].format.setForeground(keywordColor);
    }
}
