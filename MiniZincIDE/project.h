#ifndef PROJECT_H
#define PROJECT_H

#include <QSet>
#include <QStandardItemModel>
#include <QTreeView>

class Project : public QStandardItemModel
{
    Q_OBJECT
public:
    Project();
    void setRoot(QTreeView* treeView, const QString& fileName);
    void addFile(QTreeView* treeView, const QString& fileName);
    void removeFile(const QString& fileName);
    QList<QString> files(void) const { return _files.keys(); }
    QString fileAtIndex(const QModelIndex& index);
    virtual Qt::ItemFlags flags(const QModelIndex& index) const;
    QStringList dataFiles(void) const;
    void setRunningModel(const QModelIndex& index);
    void setEditable(const QModelIndex& index);
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole );
    bool isProjectFile(const QModelIndex& index) { return projectFile->index()==index; }
signals:
    void fileRenamed(const QString& oldName, const QString& newName);
protected:
    QString projectRoot;
    QMap<QString, QModelIndex> _files;
    QStandardItem* projectFile;
    QStandardItem* mzn;
    QStandardItem* dzn;
    QStandardItem* other;
    QModelIndex runningModel;
    QModelIndex editable;

};

#endif // PROJECT_H
