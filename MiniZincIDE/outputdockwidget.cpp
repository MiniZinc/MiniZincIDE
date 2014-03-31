#include "outputdockwidget.h"
#include <QCloseEvent>

void OutputDockWidget::closeEvent(QCloseEvent *event) {
    if (isFloating())
        setFloating(false);
    else
        hide();
    event->ignore();
}
