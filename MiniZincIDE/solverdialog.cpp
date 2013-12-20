#include "solverdialog.h"
#include "ui_solverdialog.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>

SolverDialog::SolverDialog(QVector<Solver>& solvers0, const QString& mznPath, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SolverDialog),
    solvers(solvers0)
{
    ui->setupUi(this);

    ui->mznDistribPath->setText(mznPath);
    for (int i=solvers.size(); i--;) {
        ui->solvers_combo->insertItem(0,solvers[i].name,i);
    }
    ui->solvers_combo->setCurrentIndex(0);
}

SolverDialog::~SolverDialog()
{
    delete ui;
}

QString SolverDialog::mznPath()
{
    return ui->mznDistribPath->text();
}

void SolverDialog::on_solvers_combo_currentIndexChanged(int index)
{
    if (index<solvers.size()) {
        ui->name->setText(solvers[index].name);
        ui->executable->setText(solvers[index].executable);
        ui->mznpath->setText(solvers[index].mznlib);
        ui->backend->setText(solvers[index].backend);

        ui->updateButton->setText("Update");
        ui->deleteButton->setEnabled(true);
        ui->solverFrame->setEnabled(!solvers[index].builtin);
    } else {
        ui->name->setText("");
        ui->executable->setText("");
        ui->mznpath->setText("");
        ui->backend->setText("");
        ui->solverFrame->setEnabled(true);
        ui->updateButton->setText("Add");
        ui->deleteButton->setEnabled(false);
    }
}

void SolverDialog::on_updateButton_clicked()
{
    int index = ui->solvers_combo->currentIndex();
    if (index==solvers.size()) {
        Solver s;
        solvers.append(s);
    }
    solvers[index].backend = ui->backend->text();
    solvers[index].executable = ui->executable->text();
    solvers[index].mznlib = ui->mznpath->text();
    solvers[index].name = ui->name->text();
    solvers[index].builtin = false;
    if (index==solvers.size()-1) {
        ui->solvers_combo->insertItem(index,ui->name->text(),index);
    }
    ui->solvers_combo->setCurrentIndex(index);
}

void SolverDialog::on_deleteButton_clicked()
{
    int index = ui->solvers_combo->currentIndex();
    if (QMessageBox::warning(this,"MiniZinc IDE","Delete solver "+solvers[index].name+"?",QMessageBox::Ok | QMessageBox::Cancel)
            == QMessageBox::Ok) {
        solvers.remove(index);
        ui->solvers_combo->removeItem(index);
    }
}

void SolverDialog::on_mznpath_select_clicked()
{
    QFileDialog fd(this,"Select MiniZinc distribution path (bin directory)");
    QDir dir(ui->mznDistribPath->text());
    fd.setDirectory(dir);
    fd.setFileMode(QFileDialog::Directory);
    fd.setOption(QFileDialog::ShowDirsOnly, true);
    if (fd.exec()) {
        ui->mznDistribPath->setText(fd.selectedFiles().first());
    }
}

void SolverDialog::on_exec_select_clicked()
{
    QFileDialog fd(this,"Select solver executable");
    fd.selectFile(ui->executable->text());
    fd.setFileMode(QFileDialog::ExistingFile);
    if (fd.exec()) {
        ui->executable->setText(fd.selectedFiles().first());
    }
}
