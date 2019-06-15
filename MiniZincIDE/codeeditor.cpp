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
    connect(document(), SIGNAL(contentsChanged()), this, SLOT(contentsChanged()));

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
    QPlainTextEdit(parent), loadContentsButton(NULL), tabs(t), darkMode(darkMode0), modifiedSinceLastCheck(true)
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
    completer = new QCompleter(this);
    QStringList completionList;
    completionList << "annotation" << "array" << "bool" << "case" << "constraint" << "default"
                   << "diff" << "else" << "elseif" << "endif" << "enum" << "float"
                   << "function" << "include" << "intersect" << "maximize" << "minimize"
                   << "output" << "predicate" << "satisfy" << "solve" << "string"
                   << "subset" << "superset" << "symdiff" << "test" << "then"
                   << "tuple" << "type" << "union" << "variant_record" << "where";
    completionList.sort();
    completionModel.setStringList(completionList);
    completer->setModel(&completionModel);
    completer->setCaseSensitivity(Qt::CaseSensitive);
    completer->setModelSorting(QCompleter::CaseSensitivelySortedModel);
    completer->setWrapAround(false);
    completer->setWidget(this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    QObject::connect(completer, SIGNAL(activated(QString)), this, SLOT(insertCompletion(QString)));
    setAcceptDrops(false);
    installEventFilter(this);
}

void CodeEditor::loadContents()
{
    static_cast<IDE*>(qApp)->loadLargeFile(filepath,this);
}

void CodeEditor::insertCompletion(const QString &completion)
{
    QTextCursor tc = textCursor();
    int extra = completion.length() - completer->completionPrefix().length();
    tc.movePosition(QTextCursor::Left);
    tc.movePosition(QTextCursor::EndOfWord);
    tc.insertText(completion.right(extra));
    setTextCursor(tc);
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
    disconnect(this, SIGNAL(blockCountChanged(int)), this, SLOT(setLineNumbersWidth(int)));
    disconnect(this, SIGNAL(updateRequest(QRect,int)), this, SLOT(setLineNumbers(QRect,int)));
    disconnect(this, SIGNAL(cursorPositionChanged()), this, SLOT(cursorChange()));
    disconnect(this->document(), SIGNAL(modificationChanged(bool)), this, SLOT(docChanged(bool)));
    QList<QTextEdit::ExtraSelection> noSelections;
    setExtraSelections(noSelections);
    QPlainTextEdit::setDocument(document);
    if (document) {
        QFont f= font();
        highlighter = new Highlighter(f,darkMode,document);
    }
    connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(setLineNumbersWidth(int)));
    connect(this, SIGNAL(updateRequest(QRect,int)), this, SLOT(setLineNumbers(QRect,int)));
    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(cursorChange()));
    connect(this->document(), SIGNAL(modificationChanged(bool)), this, SLOT(docChanged(bool)));
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

Highlighter& CodeEditor::getHighlighter() {
    return *highlighter;
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

void CodeEditor::contentsChanged()
{
    modifiedSinceLastCheck = true;
}

void CodeEditor::keyPressEvent(QKeyEvent *e)
{
    if (completer->popup()->isVisible()) {
        switch (e->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return; // let the completer do default behavior
        default:
            break;
        }
    }
    if (e->key() == Qt::Key_Tab) {
        e->accept();
        QTextCursor cursor(textCursor());
        cursor.insertText("  ");
        ensureCursorVisible();
    } else if (e->key() == Qt::Key_Return) {
        e->accept();
        QTextCursor cursor(textCursor());
        QString curLine = cursor.block().text();
        QRegExp leadingWhitespace("^(\\s*)");
        cursor.insertText("\n");
        if (leadingWhitespace.indexIn(curLine) != -1) {
            cursor.insertText(leadingWhitespace.cap(1));
        }
        ensureCursorVisible();
    } else {
        bool isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_E); // CTRL+E
        if (!isShortcut) // do not process the shortcut when we have a completer
            QPlainTextEdit::keyPressEvent(e);
        const bool ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
        if (ctrlOrShift && e->text().isEmpty())
            return;
        static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-="); // end of word
        bool hasModifier = (e->modifiers() != Qt::NoModifier) && !ctrlOrShift;

        QTextCursor tc = textCursor();
        tc.select(QTextCursor::WordUnderCursor);
        QString completionPrefix = tc.selectedText();
        if (!isShortcut && (hasModifier || e->text().isEmpty()|| completionPrefix.length() < 3
                            || eow.contains(e->text().right(1)))) {
            completer->popup()->hide();
            return;
        }
        if (completionPrefix != completer->completionPrefix()) {
            completer->setCompletionPrefix(completionPrefix);
            completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
        }
        QRect cr = cursorRect();
        cr.setWidth(completer->popup()->sizeHintForColumn(0)
                    + completer->popup()->verticalScrollBar()->sizeHint().width());
        completer->complete(cr); // popup it up!
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
    QList<QTextEdit::ExtraSelection> allExtraSels = extraSelections();

    QList<QTextEdit::ExtraSelection> extraSelections;
    foreach (QTextEdit::ExtraSelection sel, allExtraSels) {
        if (sel.format.underlineColor()==Qt::red) {
            extraSelections.append(sel);
        }
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
    QColor backgroundColor;
    QColor foregroundActiveColor;
    QColor foregroundInactiveColor;
    if (darkMode) {
        backgroundColor = QColor(0x26, 0x26, 0x26);
        foregroundActiveColor = Qt::white;
        foregroundInactiveColor = Qt::darkGray;
    } else {
        backgroundColor = QColor(Qt::lightGray).lighter(120);
        foregroundActiveColor = Qt::black;
        foregroundInactiveColor = Qt::gray;
    }

    QPainter painter(lineNumbers);
    QFont lineNoFont = font();
    QFontMetrics fm(lineNoFont);
    int origFontHeight = fm.height();
    lineNoFont.setPointSizeF(lineNoFont.pointSizeF()*0.8);
    QFontMetrics fm2(lineNoFont);
    int heightDiff = (origFontHeight-fm2.height());
    painter.setFont(lineNoFont);
    painter.fillRect(event->rect(), backgroundColor);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int) blockBoundingRect(block).height();

    int curLine = textCursor().blockNumber();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            if (errorLines.contains(blockNumber)) {
                painter.setPen(Qt::red);
            } else if (blockNumber == curLine) {
                painter.setPen(foregroundActiveColor);
            } else {
                painter.setPen(foregroundInactiveColor);
            }
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
    } else if (ev->type() == QEvent::ToolTip) {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(ev);
        QPoint evPos = helpEvent->pos();
        evPos = QPoint(evPos.x()-lineNumbersWidth(),evPos.y());
        if (evPos.x() >= 0) {
            QTextCursor cursor = cursorForPosition(evPos);
            cursor.movePosition(QTextCursor::PreviousCharacter);
            bool foundError = false;
            foreach (CodeEditorError cee, errors) {
                if (cursor.position() >= cee.startPos && cursor.position() <= cee.endPos) {
                    QToolTip::showText(helpEvent->globalPos(), cee.msg);
                    foundError = true;
                    break;
                }
            }
            if (!foundError) {
                cursor.select(QTextCursor::WordUnderCursor);
                QString loc(filename+":");
                loc += QString().number(cursor.block().blockNumber()+1);
                loc += ".";
                loc += QString().number(cursor.selectionStart()-cursor.block().position()+1);
                QHash<QString,QString>::iterator idMapIt = idMap.find(loc);
                if (idMapIt != idMap.end()) {
                    QToolTip::showText(helpEvent->globalPos(), idMapIt.value());
                } else {
                    QToolTip::hideText();
                }
            }
        } else {
            QToolTip::hideText();
        }
        return true;
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

#ifdef MINIZINC_IDE_HAVE_LIBMINIZINC

class CollectIds : public MiniZinc::EVisitor {
public:
    QHash<QString,QString>& ids;
    QSet<QString> functionIds;
    MiniZinc::EnvI& env;
    CollectIds(QHash<QString,QString>& ids0, MiniZinc::EnvI& env0)
        : ids(ids0), env(env0) {}
    void vId(const MiniZinc::Id& id) {
        QString loc = QString::fromStdString(id.loc().filename().str()+":");
        loc += QString().number(id.loc().first_line())+"."+QString().number(id.loc().first_column());
        std::ostringstream oss;
        oss << *id.decl();
        ids.insert(loc,QString::fromStdString(oss.str()));
    }
    void vCall(const MiniZinc::Call& call) {
        QString loc = QString::fromStdString(call.loc().filename().str()+":");
        loc += QString().number(call.loc().first_line())+"."+QString().number(call.loc().first_column());
        if (call.decl()) {
            std::ostringstream oss;
            oss << *call.decl();
            ids.insert(loc,QString::fromStdString(oss.str()));
        } else {
            ids.insert(loc,QString::fromStdString(call.id().str()+" is "+call.type().nonEnumToString()));
        }
    }

};

class IterCollectIds : public MiniZinc::ItemVisitor {
public:
    CollectIds& cids;
    IterCollectIds(CollectIds& cids0) : cids(cids0) {}

    /// Visit variable declaration
    void vVarDeclI(MiniZinc::VarDeclI* i) {
        MiniZinc::bottomUp<CollectIds>(cids,i->e()->id());
        MiniZinc::bottomUp<CollectIds>(cids,i->e());
    }
    /// Visit assign item
    void vAssignI(MiniZinc::AssignI* ai) { MiniZinc::bottomUp<CollectIds>(cids, ai->e()); }
    /// Visit constraint item
    void vConstraintI(MiniZinc::ConstraintI* ci) { MiniZinc::bottomUp<CollectIds>(cids, ci->e()); }
    /// Visit solve item
    void vSolveI(MiniZinc::SolveI* si) {
        for (MiniZinc::ExpressionSetIter it = si->ann().begin(); it != si->ann().end(); ++it) {
            MiniZinc::bottomUp<CollectIds>(cids, *it);
        }
        MiniZinc::bottomUp<CollectIds>(cids, si->e());
    }
    /// Visit output item
    void vOutputI(MiniZinc::OutputI* oi) { MiniZinc::bottomUp<CollectIds>(cids, oi->e()); }
    /// Visit function item
    void vFunctionI(MiniZinc::FunctionI* fi) {
        cids.functionIds.insert(fi->id().c_str());
        MiniZinc::bottomUp<CollectIds>(cids, fi->e());
    }
};

#endif

void CodeEditor::checkFile(const QVector<MiniZincError>& mznErrors)
{
    QList<QTextEdit::ExtraSelection> allExtraSels = extraSelections();

    QList<QTextEdit::ExtraSelection> extraSelections;
    foreach (QTextEdit::ExtraSelection sel, allExtraSels) {
        if (sel.format.underlineColor()!=Qt::red) {
            extraSelections.append(sel);
        }
    }

    errors.clear();
    errorLines.clear();
    for (unsigned int i=0; i<mznErrors.size(); i++) {
        QTextEdit::ExtraSelection sel;
        QTextCharFormat format = sel.format;
        format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        format.setUnderlineColor(Qt::red);
        QTextBlock block = document()->findBlockByNumber(mznErrors[i].first_line-1);
        QTextBlock endblock = document()->findBlockByNumber(mznErrors[i].last_line-1);
        if (block.isValid() && endblock.isValid()) {
            QTextCursor cursor = textCursor();
            cursor.setPosition(block.position());
            int firstCol = mznErrors[i].first_col < mznErrors[i].last_col ? (mznErrors[i].first_col-1) : (mznErrors[i].last_col-1);
            int lastCol = mznErrors[i].first_col < mznErrors[i].last_col ? (mznErrors[i].last_col) : (mznErrors[i].first_col);
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, firstCol);
            int startPos = cursor.position();
            cursor.setPosition(endblock.position(), QTextCursor::KeepAnchor);
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, lastCol);
            int endPos = cursor.position();
            sel.cursor = cursor;
            sel.format = format;
            extraSelections.append(sel);
            CodeEditorError cee(startPos,endPos,mznErrors[i].msg);
            errors.append(cee);
            for (int j=mznErrors[i].first_line; j<=mznErrors[i].last_line; j++) {
                errorLines.insert(j-1);
            }
        }
    }
    setExtraSelections(extraSelections);
}
