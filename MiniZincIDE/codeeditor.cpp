#include <QtWidgets>
#include "codeeditor.h"

void
CodeEditor::initUI(double fontSize)
{
    QFont font("Courier New");
    font.setStyleHint(QFont::Monospace);
    setFont(font);

    const int tabStop = 2;  // 4 characters

    QFontMetrics metrics(font);
    setTabStopWidth(tabStop * metrics.width(' '));

    lineNumberArea = new LineNumberArea(this);

    connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(updateLineNumberAreaWidth(int)));
    connect(this, SIGNAL(updateRequest(QRect,int)), this, SLOT(updateLineNumberArea(QRect,int)));
    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(highlightCurrentLine()));

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
    highlighter = new Highlighter(fontSize,document());

    QTextCursor cursor(textCursor());
    cursor.movePosition(QTextCursor::Start);
    setTextCursor(cursor);
    ensureCursorVisible();
    setFocus();
}

CodeEditor::CodeEditor(QFile& file, double fontSize, QWidget *parent) :
    QPlainTextEdit(parent)
{
    initUI(fontSize);
    if (file.isOpen()) {
        setPlainText(file.readAll());
        filepath = QFileInfo(file).absoluteFilePath();
        filename = QFileInfo(file).fileName();
    } else {
        filepath = "";
        filename = "Untitled";
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

int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 3 + fontMetrics().width(QLatin1Char('9')) * digits;

    return space;
}



void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}



void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}



void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}



void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(Qt::yellow).lighter(160);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

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

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);


    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int) blockBoundingRect(block).height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int) blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void CodeEditor::setFontSize(double p)
{
    QFont f(font());
    f.setPointSize(p);
    setFont(f);
    highlighter->setFontSize(p);
    highlighter->rehighlight();
}
