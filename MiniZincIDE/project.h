#ifndef PROJECT_H
#define PROJECT_H

#include <QStringList>
#include <QStandardItemModel>

class Project : public QStandardItemModel
{
public:
    Project();
    void setRoot(const QString& fileName);
    void addFile(const QString& fileName);
    int noOfFiles(void) const { return files.size(); }
    QString file(int i) const { return files[i]; }
protected:
    QString projectRoot;
    QStringList files;
    QStandardItem* mzn;
    QStandardItem* dzn;
    QStandardItem* fzn;
    QStandardItem* other;
};

#endif // PROJECT_H
