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

void Project::addFile(QTreeView* treeView, const QString &fileName)
{
    if (_files.contains(fileName))
        return;
    _files << fileName;
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
    treeView->expand(curItem->index());
    curItem = curItem->child(0);
    int i=0;
    while (curItem != NULL) {
        if (curItem->text() == path.first()) {
            path.pop_front();
            treeView->expand(curItem->index());
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
        treeView->expand(newItem->index());
        prevItem = newItem;
    }
}

QString Project::fileAtIndex(const QModelIndex &index)
{
    QStandardItem* item = itemFromIndex(index);
    if (item->hasChildren())
        return "";
    QString fileName;
    while (item != NULL && item->parent() != NULL && item->parent() != invisibleRootItem() ) {
        if (fileName.isEmpty())
            fileName = item->text();
        else
            fileName = item->text()+"/"+fileName;
        item = item->parent();
    }
    if (fileName.isEmpty())
        return "";
    if (!projectRoot.isEmpty()) {
        return QFileInfo(projectRoot).absolutePath()+"/"+fileName;
    }
    return fileName;
}

Qt::ItemFlags Project::flags(const QModelIndex&) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}
