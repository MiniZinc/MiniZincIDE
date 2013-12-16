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
