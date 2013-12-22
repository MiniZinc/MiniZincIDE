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

#ifndef FZNDOC_H
#define FZNDOC_H

#include <QObject>

class FznDoc : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString str READ str WRITE setstr)
public:
    explicit FznDoc(QObject *parent = 0);

    void setstr(const QString& s) {
        m_str =s ;
    }
    QString str(void) {
        return m_str;
    }

private:
    QString m_str;
};

#endif // FZNDOC_H
