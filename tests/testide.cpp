#include <QtTest>
#include <QSettings>

#include "testide.h"
#include "ide.h"

 void TestMocker::resetSettings() {
    QSettings settings;
    settings.clear();
    settings.beginGroup("ide");
    // Suppress prompt for enabling version check
    settings.setValue("lastCheck21", QDate::currentDate().toString());
    settings.setValue("uuid", QUuid::createUuid().toString());
    settings.setValue("checkforupdates21", false);
    settings.sync();
}

int main(int argc, char *argv[])
{
    IDE a(argc, argv);
    return QTest::qExec(new TestIDE, argc, argv);
}
