/*
 *  Author:
 *     Guido Tack <guido.tack@monash.edu>
 *
 *  Copyright:
 *     NICTA 2013
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "paramdialog.h"
#include "ui_paramdialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QFileInfo>
#include <QDebug>
#include <QScrollArea>

ParamDialog::ParamDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ParamDialog)
{
    ui->setupUi(this);
}

int ParamDialog::getModel(const QStringList& modelFiles) {
    setWindowTitle("Select model to run");
    selectedFiles = new QListWidget(this);
    selectedFiles->setSelectionMode(QAbstractItemView::SingleSelection);
    for (int i=0; i<modelFiles.size(); i++) {
        QFileInfo fi(modelFiles[i]);
        QListWidgetItem* lwi = new QListWidgetItem(fi.fileName());
        selectedFiles->addItem(lwi);
        if (previousModelFile==modelFiles[i]) {
            selectedFiles->setCurrentItem(lwi);
        }
    }
    if (selectedFiles->selectedItems().size()==0) {
        selectedFiles->item(0)->setSelected(true);
    }
    QFormLayout* mainLayout = new QFormLayout;
    mainLayout->addRow(selectedFiles);
    ui->frame->setLayout(mainLayout);
    int selectedModel = -1;
    selectedFiles->setFocus();
    if (QDialog::exec()==QDialog::Accepted) {
        selectedModel = selectedFiles->currentRow();
        previousModelFile=modelFiles[selectedModel];
    }
    delete ui->frame;
    ui->frame = new QFrame;
    ui->frame->setFrameShape(QFrame::StyledPanel);
    ui->frame->setFrameShadow(QFrame::Raised);
    ui->verticalLayout->insertWidget(0, ui->frame);
    return selectedModel;
}

void ParamDialog::getParams(QStringList params, const QStringList& dataFiles, QStringList &values, QStringList &additionalDataFiles)
{
    setWindowTitle("Model parameters");
    QVector<QLineEdit*> le;

    QFormLayout* mainLayout = new QFormLayout;

    QTabWidget* tw = new QTabWidget(this);

    QWidget* manual = new QWidget;
    auto* formLayout = new QFormLayout;
    manual->setLayout(formLayout);
    bool fillPrevious = previousParams == params;
    for (int i=0; i<params.size(); i++) {
        QLineEdit* e = new QLineEdit(fillPrevious ? previousValues[i] : "");
        le.append(e);
        formLayout->addRow(new QLabel(params[i]+" ="), le.back());
    }
    QScrollArea* scrollArea = new QScrollArea;
    scrollArea->setWidget(manual);
    tw->addTab(scrollArea, "Enter parameters");

    selectedFiles = nullptr;

    if (dataFiles.size() > 0) {
        selectedFiles = new QListWidget(this);
        selectedFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
        for (int i=0; i<dataFiles.size(); i++) {
            QFileInfo fi(dataFiles[i]);
            QListWidgetItem* lwi = new QListWidgetItem(fi.fileName());
            selectedFiles->addItem(lwi);
            if (previousDataFiles.contains(dataFiles[i]) || additionalDataFiles.contains(dataFiles[i])) {
                lwi->setSelected(true);
                selectedFiles->setCurrentItem(lwi);
                selectedFiles->scrollToItem(lwi);
            }
        }
        tw->addTab(selectedFiles, "");
        if (!fillPrevious || !previousWasManual) {
            tw->setCurrentIndex(1);
        }
        // Set now to workaround Qt bug where first tab doesn't appear
        tw->setTabText(1, "Select data file");
    }
    mainLayout->addRow(tw);
    ui->frame->setLayout(mainLayout);
    le[0]->setFocus();
    if (selectedFiles && selectedFiles->selectedItems().size()==0) {
        selectedFiles->item(0)->setSelected(true);
    }
    if (QDialog::exec()==QDialog::Accepted) {
        additionalDataFiles.clear();
        previousWasManual = (tw->currentIndex()==0);
        if (tw->currentIndex()==1) {
            previousDataFiles.clear();
            for (int i=0; i<dataFiles.size(); i++) {
                QListWidgetItem* lwi = selectedFiles->item(i);
                if (lwi && lwi->isSelected()) {
                    additionalDataFiles.append(dataFiles[i]);
                    previousDataFiles.append(dataFiles[i]);
                }
            }
        } else {
            for (int i=0; i<le.size(); i++)
                values << le[i]->text();
            previousParams = params;
            previousValues = values;
        }
    }
    delete ui->frame;
    ui->frame = new QFrame;
    ui->frame->setFrameShape(QFrame::StyledPanel);
    ui->frame->setFrameShadow(QFrame::Raised);
    ui->verticalLayout->insertWidget(0, ui->frame);
}

ParamDialog::~ParamDialog()
{
    delete ui;
}
