#include "projectbrowser.h"
#include "ui_projectbrowser.h"
#include "ide.h"
#include "exception.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ProjectBrowser::ProjectBrowser(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProjectBrowser)
{
    ui->setupUi(this);

    setupContextMenu();
}

ProjectBrowser::~ProjectBrowser()
{
    delete ui;
}

void ProjectBrowser::project(Project* p)
{
    proj = p;

    ui->treeView->setModel(proj->model());
    ui->treeView->expandToDepth(1);
}

void ProjectBrowser::setupContextMenu()
{
    auto contextMenu = new QMenu(this);
    contextMenu->addAction("Open file", [=] () {
        auto selected = ui->treeView->selectionModel()->selectedIndexes();
        emit openRequested(proj->getFileNames(selected));
    });

    contextMenu->addAction("Remove from project", [=] () {
        auto selected = ui->treeView->selectionModel()->selectedIndexes();
        emit removeRequested(proj->getFileNames(selected));
    });

    auto rename = contextMenu->addAction("Rename file", [=] () {
        ui->treeView->edit(ui->treeView->currentIndex());
    });

    auto runWith = contextMenu->addAction("Run model with this data", [=] () {
        auto selected = ui->treeView->selectionModel()->selectedIndexes();
        emit runRequested(proj->getFileNames(selected));
    });

    contextMenu->addAction("Add existing file(s)...", [=] () {
        auto fileNames = QFileDialog::getOpenFileNames(this, tr("Select one or more files to open"), IDE::instance()->getLastPath(), "MiniZinc Files (*.mzn *.dzn *.json)");
        proj->add(fileNames);
        if (fileNames.count()) {
            IDE::instance()->setLastPath(QFileInfo(fileNames.last()).absolutePath() + "/");
        }
    });

    connect(ui->treeView, &QWidget::customContextMenuRequested, [=] (const QPoint& pos) {
        auto selected = ui->treeView->selectionModel()->selectedIndexes();

        if (!proj || selected.empty()) {
            return;
        }

        int numModelsSelected = 0;
        int numDataSelected = 0;
        int numCheckersSelected = 0;
        int numSolverConfigsSelected = 0;
        int numOthersSelected = 0;

        for (auto& index : selected) {
            switch (proj->getType(index)) {
            case Project::Model:
                numModelsSelected++;
                break;
            case Project::Data:
                numDataSelected++;
                break;
            case Project::Checker:
                numCheckersSelected++;
                break;
            case Project::SolverConfig:
                numSolverConfigsSelected++;
                break;
            case Project::Other:
                numOthersSelected++;
                break;
            default:
                return; // No applicable actions
            }
        }

        rename->setDisabled(selected.length() > 1);
        runWith->setDisabled(numOthersSelected > 0 || numCheckersSelected > 1 || numSolverConfigsSelected > 1);

        if (numDataSelected == 0 && numCheckersSelected == 1) {
            runWith->setText("Run model with this checker");
        } else if (numDataSelected > 0) {
            runWith->setText("Run model with this data");
        } else {
            runWith->setText("Run model");
        }

        contextMenu->exec(ui->treeView->mapToGlobal(pos));
    });
}

void ProjectBrowser::on_treeView_activated(const QModelIndex &index)
{
    emit openRequested({proj->getFileName(index)});
}
