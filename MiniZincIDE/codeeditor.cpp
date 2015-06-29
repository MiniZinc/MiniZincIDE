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

#include <QtWidgets>
#include "codeeditor.h"
#include "mainwindow.h"

void
CodeEditor::initUI(QFont& font)
{
    setFont(font);

    setTabStopWidth(QFontMetrics(font).width("  "));

    lineNumbers= new LineNumbers(this);

    connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(setLineNumbersWidth(int)));
    connect(this, SIGNAL(updateRequest(QRect,int)), this, SLOT(setLineNumbers(QRect,int)));
    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(cursorChange()));
    connect(document(), SIGNAL(modificationChanged(bool)), this, SLOT(docChanged(bool)));

    setLineNumbersWidth(0);
    cursorChange();

    highlighter = new Highlighter(font,darkMode,document());
    setDarkMode(darkMode);

    QTextCursor cursor(textCursor());
    cursor.movePosition(QTextCursor::Start);
    setTextCursor(cursor);
    ensureCursorVisible();
    setFocus();
}

CodeEditor::CodeEditor(QTextDocument* doc, const QString& path, bool isNewFile, bool large,
                       QFont& font, bool darkMode0, QTabWidget* t, QWidget *parent) :
    QPlainTextEdit(parent), loadContentsButton(NULL), tabs(t), darkMode(darkMode0)
{
    if (doc) {
        QPlainTextEdit::setDocument(doc);
    }
    initUI(font);
    if (isNewFile) {
        filepath = "";
        filename = path;
    } else {
        filepath = QFileInfo(path).absoluteFilePath();
        filename = QFileInfo(path).fileName();
    }
    if (large) {
        setReadOnly(true);
        QPushButton* pb = new QPushButton("Big file. Load contents?", this);
        connect(pb, SIGNAL(clicked()), this, SLOT(loadContents()));
        loadContentsButton = pb;
    }
    setAcceptDrops(false);
    installEventFilter(this);
}

void CodeEditor::loadContents()
{
    static_cast<IDE*>(qApp)->loadLargeFile(filepath,this);
}

void CodeEditor::loadedLargeFile()
{
    setReadOnly(false);
    delete loadContentsButton;
    loadContentsButton = NULL;
}

void CodeEditor::setDocument(QTextDocument *document)
{
    if (document) {
        delete highlighter;
        highlighter = NULL;
    }
    QPlainTextEdit::setDocument(document);
    if (document) {
        QFont f= font();
        highlighter = new Highlighter(f,document);
        connect(document, SIGNAL(modificationChanged(bool)), this, SLOT(docChanged(bool)));
    }
}

void CodeEditor::setDarkMode(bool enable)
{
    darkMode = enable;
    highlighter->setDarkMode(enable);
    highlighter->rehighlight();
    if (darkMode) {
        setStyleSheet("QPlainTextEdit{color: #ffffff; background-color: #181818;}");
    } else {
        setStyleSheet("QPlainTextEdit{color: #000000; background-color: #ffffff;}");
    }
}

void CodeEditor::docChanged(bool c)
{
    int t = tabs == NULL ? -1 : tabs->indexOf(this);
    if (t != -1) {
        QString title = tabs->tabText(t);
        title = title.mid(0, title.lastIndexOf(" *"));
        if (c)
            title += " *";
        tabs->setTabText(t,title);
    }
}

void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Tab) {
        e->accept();
        QTextCursor cursor(textCursor());
        cursor.insertText("  ");
    } else {
        QPlainTextEdit::keyPressEvent(e);
    }
}

int CodeEditor::lineNumbersWidth()
{
    int width = 1;
    int bc = blockCount();
    while (bc >= 10) {
        bc /= 10;
        ++width;
    }
    width = std::max(width,3);
    return 3 + fontMetrics().width(QLatin1Char('9')) * width;
}



void CodeEditor::setLineNumbersWidth(int)
{
    setViewportMargins(lineNumbersWidth(), 0, 0, 0);
}



void CodeEditor::setLineNumbers(const QRect &rect, int dy)
{
    if (dy)
        lineNumbers->scroll(0, dy);
    else
        lineNumbers->update(0, rect.y(), lineNumbers->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        setLineNumbersWidth(0);
}



void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumbers->setGeometry(QRect(cr.left(), cr.top(), lineNumbersWidth(), cr.height()));
    if (loadContentsButton) {
        loadContentsButton->move(cr.left()+lineNumbersWidth(), cr.top());
    }
}



void CodeEditor::cursorChange()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    BracketData* bd = static_cast<BracketData*>(textCursor().block().userData());

    if (bd) {
        QVector<Bracket>& brackets = bd->brackets;
        int pos = textCursor().block().position();
        for (int i=0; i<brackets.size(); i++) {
            int curPos = textCursor().position()-textCursor().block().position();
            Bracket& b = brackets[i];
            int parenPos0 = -1;
            int parenPos1 = -1;
            int errPos = -1;
            if (b.pos == curPos-1 && (b.b == '(' || b.b == '{' || b.b == '[')) {
                parenPos1 = matchLeft(textCursor().block(), b.b, i+1, 0);
                if (parenPos1 != -1) {
                    parenPos0 = pos+b.pos;
                } else {
                    errPos = pos+b.pos;
                }
            } else if (b.pos == curPos-1 && (b.b == ')' || b.b == '}' || b.b == ']')) {
                parenPos0 = matchRight(textCursor().block(), b.b, i-1, 0);
                if (parenPos0 != -1) {
                    parenPos1 = pos+b.pos;
                } else {
                    errPos = pos+b.pos;
                }
            }
            if (parenPos0 != -1 && parenPos1 != -1) {
                QTextEdit::ExtraSelection sel;
                QTextCharFormat format = sel.format;
                format.setBackground(Qt::green);
                sel.format = format;
                QTextCursor cursor = textCursor();
                cursor.setPosition(parenPos0);
                cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                sel.cursor = cursor;
                extraSelections.append(sel);
                cursor.setPosition(parenPos1);
                cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                sel.cursor = cursor;
                extraSelections.append(sel);
            }
            if (errPos != -1) {
                QTextEdit::ExtraSelection sel;
                QTextCharFormat format = sel.format;
                format.setBackground(Qt::red);
                sel.format = format;
                QTextCursor cursor = textCursor();
                cursor.setPosition(errPos);
                cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                sel.cursor = cursor;
                extraSelections.append(sel);
            }
        }
    }

    setExtraSelections(extraSelections);
}

int CodeEditor::matchLeft(QTextBlock block, QChar b, int i, int nLeft)
{
    QChar match;
    switch (b.toLatin1()) {
    case '(' : match = ')'; break;
    case '{' : match = '}'; break;
    case '[' : match = ']'; break;
    default: break; // should not happen
    }

    while (block.isValid()) {
        BracketData* bd = static_cast<BracketData*>(block.userData());
        QVector<Bracket>& brackets = bd->brackets;
        int docPos = block.position();
        for (; i<brackets.size(); i++) {
            Bracket& b = brackets[i];
            if (b.b=='(' || b.b=='{' || b.b=='[') {
                nLeft++;
            } else if (b.b==match && nLeft==0) {
                return docPos+b.pos;
            } else {
                nLeft--;
            }
        }
        block = block.next();
        i = 0;
    }
    return -1;
}

int CodeEditor::matchRight(QTextBlock block, QChar b, int i, int nRight)
{
    QChar match;
    switch (b.toLatin1()) {
    case ')' : match = '('; break;
    case '}' : match = '{'; break;
    case ']' : match = '['; break;
    default: break; // should not happen
    }
    if (i==-1)
        block = block.previous();
    while (block.isValid()) {
        BracketData* bd = static_cast<BracketData*>(block.userData());
        QVector<Bracket>& brackets = bd->brackets;
        if (i==-1)
            i = brackets.size()-1;
        int docPos = block.position();
        for (; i>-1 && brackets.size()>0; i--) {
            Bracket& b = brackets[i];
            if (b.b==')' || b.b=='}' || b.b==']') {
                nRight++;
            } else if (b.b==match && nRight==0) {
                return docPos+b.pos;
            } else {
                nRight--;
            }
        }
        block = block.previous();
        i = -1;
    }
    return -1;
}

void CodeEditor::paintLineNumbers(QPaintEvent *event)
{
    QPainter painter(lineNumbers);
    QFont lineNoFont = font();
    QFontMetrics fm(lineNoFont);
    int origFontHeight = fm.height();
    lineNoFont.setPointSizeF(lineNoFont.pointSizeF()*0.8);
    QFontMetrics fm2(lineNoFont);
    int heightDiff = (origFontHeight-fm2.height());
    painter.setFont(lineNoFont);
    painter.fillRect(event->rect(), QColor(Qt::lightGray).lighter(120));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int) blockBoundingRect(block).height();

    int curLine = textCursor().blockNumber();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            if (blockNumber == curLine)
                painter.setPen(Qt::black);
            else
                painter.setPen(Qt::gray);
            int textTop = top+fontMetrics().leading()+heightDiff;
            painter.drawText(0, textTop, lineNumbers->width(), fm2.height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int) blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void CodeEditor::setEditorFont(QFont& font)
{
    setFont(font);
    document()->setDefaultFont(font);
    highlighter->setEditorFont(font);
    highlighter->rehighlight();
}

bool CodeEditor::eventFilter(QObject *, QEvent *ev)
{
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(ev);
        if (keyEvent == QKeySequence::Copy) {
            copy();
            return true;
        } else if (keyEvent == QKeySequence::Cut) {
            cut();
            return true;
        }
    }
    return false;
}

void CodeEditor::copy()
{
    highlighter->copyHighlightedToClipboard(textCursor());
}


void CodeEditor::cut()
{
    highlighter->copyHighlightedToClipboard(textCursor());
    textCursor().removeSelectedText();
}
