#include "project.h"

#include <QFileInfo>
#include <QDir>
#include <QDebug>

Project::Project()
{

    mzn = new QStandardItem("Models");
    invisibleRootItem()->appendRow(mzn);
    dzn = new QStandardItem("Data");
    invisibleRootItem()->appendRow(dzn);
    fzn = new QStandardItem("FlatZinc");
    invisibleRootItem()->appendRow(fzn);
    other = new QStandardItem("Other");
    invisibleRootItem()->appendRow(other);
}

void Project::setRoot(const QString &fileName)
{
    projectRoot = fileName;
}

void Project::addFile(const QString &fileName)
{
    files << fileName;
    QFileInfo fi(fileName);
    QString absFileName = fi.absoluteFilePath();
    QString relFileName;
    if (!projectRoot.isEmpty()) {
        QDir projectDir(QFileInfo(projectRoot).absoluteDir());
        relFileName = projectDir.relativeFilePath(absFileName);
    } else {
        relFileName = absFileName;
    }

    QStringList path = relFileName.split(QDir::separator());
    while (path.first().isEmpty()) {
        path.pop_front();
    }
    QStandardItem* curItem;
    bool isMiniZinc = true;
    if (fi.suffix()=="mzn") {
        curItem = mzn;
    } else if (fi.suffix()=="dzn") {
        curItem = dzn;
    } else if (fi.suffix()=="fzn") {
        curItem = fzn;
    } else {
        curItem = other;
        isMiniZinc = false;
    }
    QStandardItem* prevItem = curItem;
    curItem = curItem->child(0);
    int i=0;
    while (curItem != NULL) {
        if (curItem->text() == path.first()) {
            path.pop_front();
            prevItem = curItem;
            curItem = curItem->child(0);
            i = 0;
        } else {
            i += 1;
            curItem = curItem->parent()->child(i);
        }
    }
    for (int i=0; i<path.size(); i++) {
        QStandardItem* newItem = new QStandardItem(path[i]);
        if (i<path.size()-1) {
            newItem->setIcon(QIcon(":/icons/images/folder.png"));
        } else if (isMiniZinc) {
            newItem->setIcon(QIcon(":/images/mznicon.png"));
        }
        prevItem->appendRow(newItem);
        prevItem = newItem;
    }
}
