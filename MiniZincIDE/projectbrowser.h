#ifndef PROJECTBROWSER_H
#define PROJECTBROWSER_H

#include <QMenu>
#include <QDir>
#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include "project.h"
#include "moocsubmission.h"

namespace Ui {
class ProjectBrowser;
}

class ProjectBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectBrowser(QWidget *parent = nullptr);
    ~ProjectBrowser();

    ///
    /// \brief Gets the project associated with this project browser
    /// \return A pointer to the project
    ///
    Project* project(void)
    {
        return proj;
    }
    ///
    /// \brief Sets the project associated with this project browser
    /// \param project The new project
    ///
    void project(Project* project);

signals:
    void runRequested(const QStringList& files);
    void openRequested(const QStringList& files);
    void removeRequested(const QStringList& files);

private slots:
    void on_treeView_activated(const QModelIndex &index);

private:
    Ui::ProjectBrowser *ui;

    void setupContextMenu(void);

    Project* proj = nullptr;
};

#endif // PROJECTBROWSER_H
