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

#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QTabWidget>
#include <QCompleter>
#include <QStringListModel>
#include <QTimer>

#include "highlighter.h"
#include "theme.h"

class CodeEditorError {
public:
    int startPos;
    int endPos;
    QString msg;
    CodeEditorError(int startPos0, int endPos0, const QString& msg0)
        : startPos(startPos0), endPos(endPos0), msg(msg0) {}
};

struct MiniZincError {
    bool isWarning;
    QString filename;
    int first_line;
    int last_line;
    int first_col;
    int last_col;
    QString msg;
};

class EditorHeader;

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodeEditor(QTextDocument* doc, const QString& path, bool isNewFile, bool large,
                        QFont& font, int indentSize, bool useTabs, const Theme& theme, bool darkMode,
                        QTabWidget* tabs, QWidget *parent);
    void paintLineNumbers(QPaintEvent *event);
    int lineNumbersWidth();
    void paintDebugInfo(QPaintEvent *event);
    void paintHeader(QPaintEvent *event);
    int debugInfoWidth();
    int debugInfoOffset();
    QString filepath;
    QString filename;
    QString playgroundTempFile;
    void setEditorFont(QFont& font);
    void setDocument(QTextDocument *document);
    void setTheme(const Theme& t, bool d);
    void setIndentSize(int size);
    void setIndentTab(bool useTabs0) { useTabs = useTabs0; }
    Highlighter& getHighlighter();
    void checkFile(const QVector<MiniZincError>& errors);
    void showDebugInfo(bool show);
    const static int DEBUG_TAB_SIZE = 70;
    void shiftLeft();
    void shiftRight();
protected:
    void resizeEvent(QResizeEvent *event);
    void showEvent(QShowEvent *event);
    void initUI(QFont& font);
    virtual void keyPressEvent(QKeyEvent *e);
    bool eventFilter(QObject *, QEvent *);
private slots:
    void setViewportWidth(int newBlockCount);
    void cursorChange();
    void setLineNumbers(const QRect &, int);
    void setDebugInfoPos(const QRect &, int);
    void docChanged(bool);
    void contentsChanged();
    void contentsChangedWithTimeout();
    void loadContents();
    void insertCompletion(const QString& completion);
private:
    QWidget* lineNumbers;
    QWidget* debugInfo;
    EditorHeader* editorHeader;
    QWidget* loadContentsButton;
    QTabWidget* tabs;
    Highlighter* highlighter;
    QCompleter* completer;
    QStringListModel completionModel;
    int indentSize;
    bool useTabs;
    QList<CodeEditorError> errors;
    QSet<int> errorLines;
    QSet<int> warningLines;
    QHash<QString,QString> idMap;
    QTimer modificationTimer;
    Theme theme;
    bool darkMode;
    int matchLeft(QTextBlock block, QChar b, int i, int n);
    int matchRight(QTextBlock block, QChar b, int i, int n);

    void shiftSelected(int amount);

    QColor interpolate(QColor start,QColor end,double ratio); // This should not go here
    QColor heatColor(double ratio); // This should not go here

signals:
    void escPressed();
    void changedDebounced();
public slots:
    void loadedLargeFile();
    void copy();
    void cut();
};

class LineNumbers: public QWidget
{
public:
    LineNumbers(CodeEditor *e) : QWidget(e), codeEditor(e) {}

    QSize sizeHint() const {
        return QSize(codeEditor->lineNumbersWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) {
        codeEditor->paintLineNumbers(event);
    }

private:
    CodeEditor *codeEditor;
};

class DebugInfo: public QWidget
{
public:
    DebugInfo(CodeEditor *e) : QWidget(e), codeEditor(e) {}

    QSize sizeHint() const {
        return QSize(codeEditor->debugInfoWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) {
        codeEditor->paintDebugInfo(event);
    }

private:
    CodeEditor *codeEditor;
};

class EditorHeader: public QWidget
{
    Q_OBJECT
public:
    EditorHeader(CodeEditor *e) : QWidget(e), codeEditor(e) {
        setMouseTracking(true);
    }

    QSize sizeHint() const {
        return QSize(0, codeEditor->debugInfoOffset());
    }

protected:
    void paintEvent(QPaintEvent *event) {
        codeEditor->paintHeader(event);
    }
private:
    CodeEditor *codeEditor;

};

#endif // CODEEDITOR_H
