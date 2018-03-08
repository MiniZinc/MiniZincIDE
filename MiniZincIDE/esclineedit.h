#ifndef ESCLINEEDIT_H
#define ESCLINEEDIT_H

#include <QLineEdit>

class EscLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    EscLineEdit(QWidget* parent = 0);
signals:
    void escPressed(void);
protected:
    void keyReleaseEvent(QKeyEvent *);
};

#endif // ESCLINEEDIT_H
