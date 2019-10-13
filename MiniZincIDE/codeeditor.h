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

class CodeEditorError {
public:
    int startPos;
    int endPos;
    QString msg;
    CodeEditorError(int startPos0, int endPos0, const QString& msg0)
        : startPos(startPos0), endPos(endPos0), msg(msg0) {}
};

struct MiniZincError {
    QString filename;
    int first_line;
    int last_line;
    int first_col;
    int last_col;
    QString msg;
};


class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodeEditor(QTextDocument* doc, const QString& path, bool isNewFile, bool large,
                        QFont& font, bool darkMode, QTabWidget* tabs, QWidget *parent);
    void paintLineNumbers(QPaintEvent *event);
    int lineNumbersWidth();
    QString filepath;
    QString filename;
    void setEditorFont(QFont& font);
    void setDocument(QTextDocument *document);
    void setDarkMode(bool);
    Highlighter& getHighlighter();
    bool modifiedSinceLastCheck;
    void checkFile(const QVector<MiniZincError>& errors);
protected:
    void resizeEvent(QResizeEvent *event);
    void initUI(QFont& font);
    virtual void keyPressEvent(QKeyEvent *e);
    bool eventFilter(QObject *, QEvent *);
private slots:
    void setLineNumbersWidth(int newBlockCount);
    void cursorChange();
    void setLineNumbers(const QRect &, int);
    void docChanged(bool);
    void contentsChanged();
    void contentsChangedWithTimeout();
    void loadContents();
    void insertCompletion(const QString& completion);
private:
    QWidget* lineNumbers;
    QWidget* loadContentsButton;
    QTabWidget* tabs;
    Highlighter* highlighter;
    QCompleter* completer;
    QStringListModel completionModel;
    bool darkMode;
    QList<CodeEditorError> errors;
    QSet<int> errorLines;
    QHash<QString,QString> idMap;
    QTimer modificationTimer;
    int matchLeft(QTextBlock block, QChar b, int i, int n);
    int matchRight(QTextBlock block, QChar b, int i, int n);
signals:
    void escPressed();
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

#endif // CODEEDITOR_H
