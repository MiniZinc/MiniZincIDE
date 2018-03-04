#include "esclineedit.h"

#include <QKeyEvent>
#include <QDebug>

EscLineEdit::EscLineEdit(QWidget* parent) : QLineEdit(parent)
{

}

void EscLineEdit::keyReleaseEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Escape) {
        emit(escPressed());
    }
}
