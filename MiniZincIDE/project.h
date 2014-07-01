#ifndef PROJECT_H
#define PROJECT_H

#include <QSet>
#include <QStandardItemModel>
#include <QTreeView>

class Project : public QStandardItemModel
{
public:
    Project();
    void setRoot(const QString& fileName);
    void addFile(QTreeView* treeView, const QString& fileName);
    const QSet<QString>& files(void) const { return _files; }
    QString fileAtIndex(const QModelIndex& index);
    virtual Qt::ItemFlags flags(const QModelIndex& index) const;
protected:
    QString projectRoot;
    QSet<QString> _files;
    QStandardItem* mzn;
    QStandardItem* dzn;
    QStandardItem* fzn;
    QStandardItem* other;
};

#endif // PROJECT_H
