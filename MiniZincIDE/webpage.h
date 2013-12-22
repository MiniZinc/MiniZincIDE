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

#ifndef WEBPAGE_H
#define WEBPAGE_H
#include <QObject>
#include <QWebPage>
 
class WebPage : public QWebPage
{
    Q_OBJECT
public:
    WebPage(QObject* parent = 0);
protected:
    void javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID);
};
#endif // WEBPAGE_H
