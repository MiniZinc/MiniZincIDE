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

#include "highlighter.h"

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
    void loadContents();
private:
    QWidget* lineNumbers;
    QWidget* loadContentsButton;
    QTabWidget* tabs;
    Highlighter* highlighter;
    bool darkMode;
    int matchLeft(QTextBlock block, QChar b, int i, int n);
    int matchRight(QTextBlock block, QChar b, int i, int n);
signals:

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
