#include "ideutils.h"

#include <QDir>
#include <QFileInfo>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QTableWidget>
#include <QPlainTextEdit>


namespace IDEUtils {

QString formatTime(qint64 time) {
    int hours =  time / 3600000;
    int minutes = (time % 3600000) / 60000;
    int seconds = (time % 60000) / 1000;
    int msec = (time % 1000);
    QString elapsed;
    if (hours > 0) {
        elapsed += QString("%1h ").arg(hours);
    }
    if (hours > 0 || minutes > 0) {
        elapsed += QString("%1m ").arg(minutes);
    }
    if (hours > 0 || minutes > 0 || seconds > 0) {
        elapsed += QString("%1s").arg(seconds);
    }
    if (hours==0 && minutes==0) {
        if (seconds > 0) {
            elapsed += " ";
        }
        elapsed += QString("%1msec").arg(msec);
    }
    return elapsed;
}

bool isChildPath(const QString& parent, const QString& child)
{
    return !QDir(parent).relativeFilePath(child).startsWith(".");
}


void watchChildChanges(QWidget* target, QObject* receiver, std::function<void()> action)
{
    for (auto widget : target->findChildren<QWidget*>()) {
        QCheckBox* checkBox;
        QLineEdit* lineEdit;
        QSpinBox* spinBox;
        QDoubleSpinBox* doubleSpinBox;
        QComboBox* comboBox;
        QGroupBox* groupBox;
        QTableWidget* tableWidget;
        QPlainTextEdit* plainTextEdit;

        if ((checkBox = qobject_cast<QCheckBox*>(widget))) {
            QObject::connect(checkBox, &QCheckBox::stateChanged, receiver, [=] (int) { action(); });
        } else if ((lineEdit = qobject_cast<QLineEdit*>(widget))) {
            QObject::connect(lineEdit, &QLineEdit::textChanged, receiver, [=] (const QString&) { action(); });
        } else if ((spinBox = qobject_cast<QSpinBox*>(widget))) {
            QObject::connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), receiver, [=] (int) { action(); });
        } else if ((doubleSpinBox = qobject_cast<QDoubleSpinBox*>(widget))) {
            QObject::connect(doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), receiver, [=] (double) { action(); });
        } else if ((comboBox = qobject_cast<QComboBox*>(widget))) {
            QObject::connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), receiver, [=] (int) { action(); });
        } else if ((groupBox = qobject_cast<QGroupBox*>(widget))) {
            QObject::connect(groupBox, &QGroupBox::toggled, receiver, [=] (bool) { action(); });
        } else if ((tableWidget = qobject_cast<QTableWidget*>(widget))) {
            QObject::connect(tableWidget, &QTableWidget::cellChanged, receiver, [=] (int, int) { action(); });
        } else if ((plainTextEdit = qobject_cast<QPlainTextEdit*>(widget))) {
            QObject::connect(plainTextEdit, &QPlainTextEdit::textChanged,receiver, [=] () { action(); });
        }
    }
}

QFont fontFromString(const QString& s)
{
    QFont font;
    if (font.fromString(s)) {
        return font;
    }

    QStringList families({"Menlo", "Consolas", "Courier New"});

    font.setPointSize(13);
    font.setStyleHint(QFont::TypeWriter);
    for (auto& family : families) {
        font.setFamily(family);
        if (font.exactMatch()) {
            break;
        }
    }
    return font;
}

}
