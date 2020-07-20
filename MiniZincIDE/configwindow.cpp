/*
 *  Main authors:
 *     Jason Nguyen <jason.nguyen@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "configwindow.h"
#include "ui_configwindow.h"

#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>

#include "process.h"
#include "exception.h"

#include <algorithm>

ConfigWindow::ConfigWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigWindow)
{
    ui->setupUi(this);
    ui->userDefinedBehavior_frame->setVisible(false);

    ui->extraParams_tableWidget->setColumnCount(3);
    ui->extraParams_tableWidget->setHorizontalHeaderLabels(QStringList() << "Parameter" << "Type" << "Value");
    ui->extraParams_tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->extraParams_tableWidget->verticalHeader()->hide();

    comboBoxModel = new QStringListModel(this);

    QList<QWidget*> toBeWatched;
    for (auto child : findChildren<QWidget*>()) {
        if (child != ui->config_comboBox) {
            toBeWatched << child;
        }
    }
    watchChanges(toBeWatched, std::bind(&ConfigWindow::invalidate, this, false));
    watchChanges(ui->options_groupBox->findChildren<QWidget*>(), std::bind(&ConfigWindow::invalidate, this, true));
}

ConfigWindow::~ConfigWindow()
{
    delete ui;
    delete comboBoxModel;
}

void ConfigWindow::init()
{
    loadConfigs();
    initialized = true;
    int i = 0;
    for (auto& config : configs) {
        if (config.solverDefinition.isDefaultSolver) {
            setCurrentIndex(i);
            break;
        }
        i++;
    }
}

void ConfigWindow::loadConfigs(void)
{
    QStringList files;
    for (auto& config : configs) {
        if (!config.paramFile.isEmpty()) {
            files.append(config.paramFile);
        }
    }
    configs.clear();
    for (auto& solver : MznDriver::get().solvers()) {
        configs.append(SolverConfiguration(solver, true));
    }
    if (!files.empty()) {
        for (auto fileName : files) {
            configs.append(SolverConfiguration::loadJSON(fileName));
        }
    }
    populateComboBox();
}

void ConfigWindow::addConfig(const QString &fileName)
{
    try {
        updateSolverConfig(configs[currentIndex()]);
        configs.append(SolverConfiguration::loadJSON(fileName));
        for (auto& sc : configs) {
            if (!sc.syncedOptionsMatch(configs[configs.length() - 1])) {
                ui->sync_checkBox->setChecked(false);
                break;
            }
        }
        populateComboBox();
        setCurrentIndex(configs.length() - 1);
    } catch (Exception& e) {
        QMessageBox::warning(this, "Parameter configuration error", e.message(), QMessageBox::Ok);
    }
}

QString ConfigWindow::saveConfig(int index, bool saveAs)
{
    auto& sc = configs[index];
    updateSolverConfig(configs[currentIndex()]);

    if (sc.paramFile.isEmpty() || saveAs) {
        QFileInfo fi(sc.paramFile);
        sc.paramFile = QFileDialog::getSaveFileName(this, "Save configuration", fi.canonicalFilePath(), "Solver configuration files (*.mpc)");
    }

    QFile f(sc.paramFile);
    f.open(QFile::WriteOnly | QFile::Truncate);
    f.write(sc.toJSON());
    f.close();

    return sc.paramFile;
}

QString ConfigWindow::saveConfig(bool saveAs)
{
    return saveConfig(currentIndex(), saveAs);
}

SolverConfiguration* ConfigWindow::currentSolverConfig(void)
{
    int i = currentIndex();
    if (i < 0 || i >= configs.length()) {
        return nullptr;
    }
    auto& sc = configs[i];
    updateSolverConfig(sc);
    return &sc;
}

const QVector<SolverConfiguration>& ConfigWindow::solverConfigs()
{
    currentSolverConfig();
    return configs;
}

int ConfigWindow::currentIndex(void)
{
    return ui->config_comboBox->currentIndex();
}

void ConfigWindow::setCurrentIndex(int i)
{
    ui->config_comboBox->setCurrentIndex(qBound(0, i, configs.length()));
}

void ConfigWindow::on_numSolutions_checkBox_stateChanged(int arg1)
{
    ui->numSolutions_spinBox->setEnabled(arg1);
}

void ConfigWindow::on_numOptimal_checkBox_stateChanged(int arg1)
{
    ui->numOptimal_spinBox->setEnabled(arg1);
}

void ConfigWindow::on_timeLimit_checkBox_stateChanged(int arg1)
{
    ui->timeLimit_doubleSpinBox->setEnabled(arg1);
}

void ConfigWindow::on_randomSeed_checkBox_stateChanged(int arg1)
{
    ui->randomSeed_lineEdit->setEnabled(arg1);
}

void ConfigWindow::on_defaultBehavior_radioButton_toggled(bool checked)
{
    ui->defaultBehavior_frame->setVisible(checked);
}

void ConfigWindow::on_userDefinedBehavior_radioButton_toggled(bool checked)
{
    ui->userDefinedBehavior_frame->setVisible(checked);
}

void ConfigWindow::on_config_comboBox_currentIndexChanged(int index)
{
    if (!initialized) {
        return;
    }

    if (lastIndex >= 0) {
        updateSolverConfig(configs[lastIndex]);
    }

    lastIndex = index;

    if (index < 0 || index >= configs.length()) {
        ui->solver_controls->setVisible(false);
        ui->options_groupBox->setVisible(false);
        ui->advanced_groupBox->setVisible(false);
        return;
    }

    auto& sc = configs[index];

    ui->solver_controls->setVisible(true);
    ui->options_groupBox->setVisible(true);
    ui->advanced_groupBox->setVisible(true);

    populating = true;

    ui->solver_label->setText(sc.solverDefinition.name + " " + sc.solverDefinition.version);

    if (!ui->sync_checkBox->isChecked()) {
        ui->timeLimit_doubleSpinBox->setValue(sc.timeLimit / 1000.0);
        ui->timeLimit_checkBox->setChecked(sc.timeLimit != 0);

        if (sc.printIntermediate && sc.numSolutions == 1) {
            ui->defaultBehavior_radioButton->setChecked(true);
        } else {
            ui->userDefinedBehavior_radioButton->setChecked(true);
        }

        ui->intermediateSolutions_checkBox->setChecked(sc.printIntermediate);

        ui->numSolutions_spinBox->setValue(sc.numSolutions);
        ui->numSolutions_checkBox->setChecked(sc.numSolutions != 0);

        ui->numOptimal_spinBox->setValue(sc.numOptimal);
        ui->numOptimal_checkBox->setChecked(sc.numOptimal != 0);

        ui->verboseCompilation_checkBox->setChecked(sc.verboseCompilation);
        ui->verboseSolving_checkBox->setChecked(sc.verboseSolving);

        ui->compilationStats_checkBox->setChecked(sc.compilationStats);
        ui->solvingStats_checkBox->setChecked(sc.solvingStats);
        ui->timingInfo_checkBox->setChecked(sc.outputTiming);
    }

    ui->intermediateSolutions_checkBox->setEnabled(
                sc.solverDefinition.stdFlags.contains("-a") ||
                sc.solverDefinition.stdFlags.contains("-i"));

    ui->numSolutions_checkBox->setEnabled(sc.solverDefinition.stdFlags.contains("-a"));
    ui->numSolutions_spinBox->setEnabled(sc.solverDefinition.stdFlags.contains("-n"));

    ui->numOptimal_checkBox->setEnabled(sc.solverDefinition.stdFlags.contains("-a-o"));
    ui->numOptimal_spinBox->setEnabled(sc.solverDefinition.stdFlags.contains("-n-o"));

    ui->verboseSolving_checkBox->setEnabled(sc.solverDefinition.stdFlags.contains("-v"));

    ui->solvingStats_checkBox->setEnabled(sc.solverDefinition.stdFlags.contains("-s"));

    ui->optimizationLevel_comboBox->setCurrentIndex(sc.optimizationLevel);
    ui->additionalData_plainTextEdit->setPlainText(sc.additionalData.join("\n"));

    ui->numThreads_spinBox->setEnabled(sc.solverDefinition.stdFlags.contains("-p"));
    ui->numThreads_spinBox->setValue(sc.numThreads);

    bool enableRandomSeed = sc.solverDefinition.stdFlags.contains("-r");
    ui->randomSeed_lineEdit->setText(sc.randomSeed.toString());
    ui->randomSeed_checkBox->setChecked(!sc.randomSeed.isNull());
    ui->randomSeed_checkBox->setEnabled(enableRandomSeed);
    ui->randomSeed_lineEdit->setEnabled(enableRandomSeed && !sc.randomSeed.isNull());

    bool enableFreeSearch = sc.solverDefinition.stdFlags.contains("-f");
    ui->freeSearch_checkBox->setEnabled(enableFreeSearch);
    ui->freeSearch_checkBox->setChecked(sc.freeSearch);

    bool hasExtraFlags = sc.solverDefinition.extraFlags.length();
    ui->extraFlags_groupBox->setVisible(hasExtraFlags);
    for (int i = ui->extraFlags_formLayout->rowCount() - 1; i >= 0; i--) {
        ui->extraFlags_formLayout->removeRow(i);
    }
    auto& used = sc.extraOptions;
    for (auto& f : sc.solverDefinition.extraFlags) {
        auto label = new QLabel(f.description);
        label->setToolTip(f.name);
//        label->setWordWrap(true);
        switch (f.t) {
        case SolverFlag::T_INT:
        {
            auto field = new QLineEdit();
            field->setAlignment(Qt::AlignRight);
            field->setPlaceholderText(f.def);
            if (used.contains(f.name)) {
                 field->setText(used[f.name].toString());
            }
            field->setValidator(new QIntValidator());
            ui->extraFlags_formLayout->addRow(label, field);
            break;
        }
        case SolverFlag::T_INT_RANGE:
        {
            auto field = new QSpinBox();
            field->setRange(static_cast<int>(f.min), static_cast<int>(f.max));
            field->setValue(used.contains(f.name) ? used[f.name].toInt() : f.def.toInt());
            ui->extraFlags_formLayout->addRow(label, field);
            break;
        }
        case SolverFlag::T_BOOL:
        {
            auto field = new QCheckBox();
            field->setChecked(used.contains(f.name) ? used[f.name].toBool() : f.def == "true");
            ui->extraFlags_formLayout->addRow(label, field);
            break;
        }
        case SolverFlag::T_BOOL_ONOFF:
        {
            auto field = new QCheckBox();
            field->setChecked(used.contains(f.name) ? used[f.name] == f.options[0] : f.def == f.options[0]);
            ui->extraFlags_formLayout->addRow(label, field);
            break;
        }
        case SolverFlag::T_FLOAT:
        {
            auto field = new QLineEdit();
            field->setAlignment(Qt::AlignRight);
            field->setPlaceholderText(f.def);
            if (used.contains(f.name)) {
                field->setText(used[f.name].toString());
            }
            field->setValidator(new QDoubleValidator());
            ui->extraFlags_formLayout->addRow(label, field);
            break;
        }
        case SolverFlag::T_FLOAT_RANGE:
        {
            auto field = new QDoubleSpinBox();
            field->setRange(f.min, f.max);
            field->setValue(used.contains(f.name) ? used[f.name].toDouble() : f.def.toDouble());
            ui->extraFlags_formLayout->addRow(label, field);
            break;
        }
        case SolverFlag::T_STRING:
        {
            auto field = new QLineEdit();
            field->setPlaceholderText(f.def);
            if (used.contains(f.name)) {
                field->setText(used[f.name].toString());
            }
            ui->extraFlags_formLayout->addRow(label, field);
            break;
        }
        case SolverFlag::T_OPT:
        {
            auto field = new QComboBox();
            field->addItems(f.options);
            field->setCurrentText(used.contains(f.name) ? used[f.name].toString() : f.def);
            ui->extraFlags_formLayout->addRow(label, field);
            break;
        }
        case SolverFlag::T_SOLVER:
        {
            auto field = new QComboBox();
            for (auto& solver : MznDriver::get().solvers()) {
                field->addItem(solver.name);
            }
            field->setCurrentText(used.contains(f.name) ? used[f.name].toString() : f.def);
            ui->extraFlags_formLayout->addRow(label, field);
            break;
        }
        }
    }
    ui->extraFlags_groupBox->setChecked(sc.useExtraOptions);

    watchChanges(ui->extraFlags_formLayout->findChildren<QWidget*>(),
                  std::bind(&ConfigWindow::invalidate, this, false));

    ui->extraParams_tableWidget->clearContents();

    for (auto it = sc.unknownOptions.begin(); it != sc.unknownOptions.end(); it++) {
        addExtraParam(it.key(), it.value());
    }

    populating = false;

    emit selectedIndexChanged(index);
}

void ConfigWindow::updateSolverConfig(SolverConfiguration& sc) {
    if (!sc.modified) {
        return;
    }

    if (ui->sync_checkBox->isChecked()) {
        for (auto& s : configs) {
            s.timeLimit = ui->timeLimit_checkBox->isChecked() ? static_cast<int>(ui->timeLimit_doubleSpinBox->value() * 1000.0) : 0;
            if (ui->defaultBehavior_radioButton->isChecked()) {
                s.printIntermediate = true;
                s.numSolutions = 1;
                s.numOptimal = 1;
            } else {
                s.printIntermediate = ui->intermediateSolutions_checkBox->isChecked();
                s.numSolutions = ui->numSolutions_checkBox->isChecked() ? ui->numSolutions_spinBox->value() : 0;
                s.numOptimal = ui->numOptimal_checkBox->isChecked() ? ui->numOptimal_spinBox->value() : 0;
            }

            s.verboseCompilation = ui->verboseCompilation_checkBox->isChecked();
            s.verboseSolving = ui->verboseSolving_checkBox->isChecked();
            s.compilationStats = ui->compilationStats_checkBox->isChecked();
            s.solvingStats = ui->solvingStats_checkBox->isChecked();
            s.outputTiming = ui->timingInfo_checkBox->isChecked();
        }
    }

    sc.optimizationLevel = ui->optimizationLevel_comboBox->currentIndex();
    QString additionalData = ui->additionalData_plainTextEdit->toPlainText();
    sc.additionalData = additionalData.length() > 0 ? additionalData.split("\n") : QStringList();
    sc.numThreads = ui->numThreads_spinBox->value();
    sc.randomSeed = ui->randomSeed_checkBox->isChecked() ? ui->randomSeed_lineEdit->text() : nullptr;
    sc.freeSearch = ui->freeSearch_checkBox->isChecked();

    sc.unknownOptions.clear();
    for (int row = 0; row < ui->extraParams_tableWidget->rowCount(); row++) {
        auto keyItem = ui->extraParams_tableWidget->item(row, 0);
        auto valueItem = ui->extraParams_tableWidget->item(row, 2);
        if (keyItem && valueItem) {
            auto key = keyItem->data(Qt::DisplayRole).toString();
            auto value = valueItem->data(Qt::DisplayRole);
            sc.unknownOptions.insert(key, value);
        }
    }

    sc.useExtraOptions = ui->extraFlags_groupBox->isChecked();
    sc.extraOptions.clear();
    int i = 0;
    for (auto& f : sc.solverDefinition.extraFlags) {
        auto field = ui->extraFlags_formLayout->itemAt(i, QFormLayout::FieldRole)->widget();
        switch (f.t) {
        case SolverFlag::T_INT:
        {
            auto value = static_cast<QLineEdit*>(field)->text();
            if (!value.isEmpty()) {
                sc.extraOptions[f.name] = value.toInt();
            }
            break;
        }
        case SolverFlag::T_INT_RANGE:
        {
            auto value = static_cast<QSpinBox*>(field)->value();
            if (value != f.def.toInt()) {
                sc.extraOptions[f.name] = value;
            }
            break;
        }
        case SolverFlag::T_BOOL:
        {
            bool value = static_cast<QCheckBox*>(field)->isChecked();
            if (value != (f.def == "true")) {
                sc.extraOptions[f.name] = value;
            }
            break;
        }
        case SolverFlag::T_BOOL_ONOFF:
        {
            auto value = static_cast<QCheckBox*>(field)->isChecked() ? f.options[0] : f.options[1];
            if (value != f.def) {
                sc.extraOptions[f.name] = value;
            }
            break;
        }
        case SolverFlag::T_FLOAT:
        {
            auto value = static_cast<QLineEdit*>(field)->text();
            if (!value.isEmpty()) {
                sc.extraOptions[f.name] = value.toDouble();
            }
            break;
        }
        case SolverFlag::T_FLOAT_RANGE:
        {
            auto value = static_cast<QDoubleSpinBox*>(field)->value();
            if (value != f.def.toDouble()) {
                sc.extraOptions[f.name] = value;
            }
            break;
        }
        case SolverFlag::T_STRING:
        {
            auto value = static_cast<QLineEdit*>(field)->text();
            if (!value.isEmpty()) {
                sc.extraOptions[f.name] = value;
            }
            break;
        }
        case SolverFlag::T_OPT:
        case SolverFlag::T_SOLVER:
        {
            auto value = static_cast<QComboBox*>(field)->currentText();
            if (value != f.def) {
                sc.extraOptions[f.name] = value;
            }
            break;
        }
        }
        i++;
    }
}


void ConfigWindow::on_addExtraParam_pushButton_clicked()
{
    addExtraParam("", "");
}

void ConfigWindow::on_removeExtraParam_pushButton_clicked()
{
    QVector<int> toBeRemoved;
    for (auto& range : ui->extraParams_tableWidget->selectedRanges()) {
        for (int i = range.topRow(); i <= range.bottomRow(); i++) {
            toBeRemoved.append(i);
        }
    }
    std::sort(toBeRemoved.begin(), toBeRemoved.end(), std::greater<>());
    for (auto i : toBeRemoved) {
        ui->extraParams_tableWidget->removeRow(i);
    }
}

void ConfigWindow::on_extraFlags_groupBox_toggled(bool arg1)
{
    ui->extraFlags_widget->setVisible(arg1);
}

void ConfigWindow::on_extraParams_tableWidget_itemSelectionChanged()
{
    bool hasSelection = ui->extraParams_tableWidget->selectedRanges().length();
    ui->removeExtraParam_pushButton->setEnabled(hasSelection);
}

void ConfigWindow::addExtraParam(const QString& key, const QVariant& value)
{
    int i = ui->extraParams_tableWidget->rowCount();
    ui->extraParams_tableWidget->insertRow(i);

    auto keyItem = new QTableWidgetItem(key.startsWith("--") ? key.right(key.length() - 2) : key);
    auto valItem = new QTableWidgetItem;
    valItem->setData(Qt::DisplayRole, value);
    ui->extraParams_tableWidget->setItem(i, 0, keyItem);
    ui->extraParams_tableWidget->setItem(i, 2, valItem);

    auto typeComboBox = new QComboBox;
    typeComboBox->addItems({"String", "Boolean", "Integer", "Float"});
    switch (value.type()) {
    case QVariant::String:
        typeComboBox->setCurrentIndex(0);
        break;
    case QVariant::Bool:
        typeComboBox->setCurrentIndex(1);
        break;
    case QVariant::Int:
        typeComboBox->setCurrentIndex(2);
        break;
    case QVariant::Double:
        typeComboBox->setCurrentIndex(3);
        break;
    default:
        typeComboBox->setCurrentIndex(0);
        break;
    }

    connect(typeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), [=] (int typeIndex) mutable {
        QVariant::Type types[] = {QVariant::String, QVariant::Bool, QVariant::Int, QVariant::Double};
        auto target = types[typeIndex];
        auto data = valItem->data(Qt::DisplayRole);
        data.convert(target);
        auto newValItem = new QTableWidgetItem;
        newValItem->setData(Qt::DisplayRole, data);
        ui->extraParams_tableWidget->setItem(valItem->row(), 2, newValItem);
        valItem = newValItem;
    });

    ui->extraParams_tableWidget->setCellWidget(i, 1, typeComboBox);
}

void ConfigWindow::watchChanges(const QList<QWidget*>& widgets, std::function<void()> action)
{
    for (auto widget : widgets) {
        QCheckBox* checkBox;
        QLineEdit* lineEdit;
        QSpinBox* spinBox;
        QDoubleSpinBox* doubleSpinBox;
        QComboBox* comboBox;
        QGroupBox* groupBox;
        QTableWidget* tableWidget;

        if ((checkBox = qobject_cast<QCheckBox*>(widget))) {
            connect(checkBox, &QCheckBox::stateChanged, [=] (int) { action(); });
        } else if ((lineEdit = qobject_cast<QLineEdit*>(widget))) {
            connect(lineEdit, &QLineEdit::textChanged, [=] (const QString&) { action(); });
        } else if ((spinBox = qobject_cast<QSpinBox*>(widget))) {
            connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), [=] (int) { action(); });
        } else if ((doubleSpinBox = qobject_cast<QDoubleSpinBox*>(widget))) {
            connect(doubleSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), [=] (double) { action(); });
        } else if ((comboBox = qobject_cast<QComboBox*>(widget))) {
            connect(comboBox, qOverload<int>(&QComboBox::currentIndexChanged), [=] (int) { action(); });
        } else if ((groupBox = qobject_cast<QGroupBox*>(widget))) {
            connect(groupBox, &QGroupBox::toggled, [=] (bool) { action(); });
        } else if ((tableWidget = qobject_cast<QTableWidget*>(widget))) {
            connect(tableWidget, &QTableWidget::cellChanged, [=] (int, int) { action(); });
        }
    }
}

void ConfigWindow::invalidate(bool all)
{
    if (populating) {
        // Ignore changes while populating the configuration widgets
        return;
    }

    bool refreshComboBox = false;

    int i = currentIndex();
    if (i >= 0 && i < configs.length() && !configs[i].modified) {
        configs[i].modified = true;
        refreshComboBox = true;
    }

    if (all && ui->sync_checkBox->isChecked()) {
        for (auto& sc : configs) {
            // Only non-builtin configs really need to be saved after modifying basic options
            if (!sc.isBuiltin && !sc.modified) {
                sc.modified = true;
                refreshComboBox = true;
            }
        }
    }

    if (refreshComboBox) {
        populateComboBox();
    }
}

void ConfigWindow::populateComboBox()
{
    bool oldInitialized = initialized;
    int oldIndex = currentIndex();
    QStringList items;
    for (auto& sc : configs) {
        QStringList label;
        label << sc.name();
        if (sc.isBuiltin) {
            label << "[built-in]";
        }
        if (sc.modified) {
            label << "*";
        }
        items << label.join(" ");
    }

    initialized = false;
    ui->config_comboBox->clear();
    ui->config_comboBox->addItems(items);
    setCurrentIndex(oldIndex);
    emit itemsChanged(items);
    initialized = oldInitialized;
}
