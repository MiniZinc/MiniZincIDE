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
#include <QMenu>
#include <QScrollBar>

#include "process.h"
#include "exception.h"

#include <algorithm>

ConfigWindow::ConfigWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigWindow)
{
    ui->setupUi(this);
    ui->userDefinedBehavior_frame->setVisible(false);

    extraFlagsMenu = new QMenu(this);
    ui->addExtraParam_toolButton->setMenu(extraFlagsMenu);

    ui->extraParams_tableWidget->setColumnCount(3);
    ui->extraParams_tableWidget->setHorizontalHeaderLabels(QStringList() << "Parameter" << "Type" << "Value");
    ui->extraParams_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->extraParams_tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->extraParams_tableWidget->verticalHeader()->hide();

    ui->extraParams_tableWidget->setItemDelegateForColumn(2, new ExtraOptionDelegate);

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
    for (auto sc : configs) {
        delete sc;
    }
}

void ConfigWindow::init()
{
    loadConfigs();
    initialized = true;
    int i = 0;
    for (auto config : configs) {
        if (config->solverDefinition.isDefaultSolver) {
            setCurrentIndex(i);
            break;
        }
        i++;
    }
}

void ConfigWindow::loadConfigs(void)
{
    try {
        QStringList files;
        for (auto config : configs) {
            if (!config->paramFile.isEmpty()) {
                files.append(config->paramFile);
            }
            delete config;
        }
        configs.clear();
        for (auto& solver : MznDriver::get().solvers()) {
            if (solver.hasAllRequiredFlags()) {
                configs.append(new SolverConfiguration(solver, true));
            }
        }
        if (!files.empty()) {
            for (auto fileName : files) {
                configs.append(new (SolverConfiguration) (SolverConfiguration::loadJSON(fileName)));
            }
        }
        populateComboBox();
        updateGUI(true);
    } catch (Exception& e) {
        QMessageBox::warning(this, "Parameter configuration error", e.message(), QMessageBox::Ok);
    }
}

bool ConfigWindow::addConfig(const QString &fileName)
{
    int index = findConfigFile(fileName);
    if (index != -1) {
        // Already have this config
        setCurrentIndex(index);
        return true;
    }

    try {
        updateSolverConfig(configs[currentIndex()]);
        configs.append(new (SolverConfiguration) (SolverConfiguration::loadJSON(fileName)));
        for (auto sc : configs) {
            if (!sc->syncedOptionsMatch(*(configs.last()))) {
                ui->sync_checkBox->setChecked(false);
                break;
            }
        }
        populateComboBox();
        setCurrentIndex(configs.length() - 1);
        return true;
    } catch (Exception& e) {
        QMessageBox::warning(this, "Parameter configuration error", e.message(), QMessageBox::Ok);
        return false;
    }
}

void ConfigWindow::mergeConfigs(const QList<SolverConfiguration*> confs)
{
    for (auto sc : confs) {
        int swapAt = sc->isBuiltin ?
                    findBuiltinConfig(sc->solverDefinition.id, sc->solverDefinition.version) :
                    findConfigFile(sc->paramFile);

        sc->modified = true;
        if (swapAt == -1) {
            sc->isBuiltin = false;
            configs.append(sc);
        } else {
            delete configs[swapAt];
            configs.replace(swapAt, sc);
        }
    }
    updateGUI(true);
    populateComboBox();
}

int ConfigWindow::findBuiltinConfig(const QString &id, const QString &version)
{
    int matchId = -1;
    int i = 0;
    for (auto sc : configs) {
        if (sc->isBuiltin && sc->solverDefinition.id == id) {
            matchId = i;
            if (sc->solverDefinition.version == version) {
                return i;
            }
        }
        i++;
    }
    return matchId;
}

int ConfigWindow::findConfigFile(const QString &file)
{
    int i = 0;
    for (auto sc : configs) {
        if (sc->paramFile == file) {
            return i;
        }
        i++;
    }
    return -1;
}

QString ConfigWindow::saveConfig(int index)
{
    auto sc = configs[index];
    auto oldSc = sc;
    updateSolverConfig(configs[currentIndex()]);

    if (sc->isBuiltin) {
        // Must clone built-in configs
        sc = new SolverConfiguration(*sc);
    }

    QString target = sc->paramFile;
    if (sc->paramFile.isEmpty()) {
        QFileInfo fi(sc->paramFile);
        target = QFileDialog::getSaveFileName(this,
                                              "Save configuration",
                                              fi.canonicalFilePath(),
                                              "Solver configuration files (*.mpc)");
    }

    if (target.isEmpty()) {
        return "";
    }

    QFile f(target);
    f.open(QFile::WriteOnly | QFile::Truncate);
    f.write(sc->toJSON());
    f.close();

    oldSc->modified = false;
    sc->modified = false;
    sc->paramFile = target;
    populateComboBox();

    emit configSaved(target);
    return target;
}

QString ConfigWindow::saveConfig()
{
    return saveConfig(currentIndex());
}

SolverConfiguration* ConfigWindow::currentSolverConfig(void)
{
    int i = currentIndex();
    if (i < 0 || i >= configs.length()) {
        return nullptr;
    }
    auto sc = configs[i];
    updateSolverConfig(sc);
    return sc;
}

const QList<SolverConfiguration*>& ConfigWindow::solverConfigs()
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

    updateGUI();

    if (index >= 0 && index < configs.length()) {
        emit selectedIndexChanged(index);
        emit selectedSolverConfigurationChanged(configs[index]);
    }
}

void ConfigWindow::updateGUI(bool overrideSync)
{
    int index = currentIndex();

    if (configs.isEmpty()) {
        ui->solver_controls->setVisible(false);
        ui->options_groupBox->setVisible(false);
        ui->advanced_groupBox->setVisible(false);
        return;
    }

    auto sc = configs[index];

    ui->solver_controls->setVisible(true);
    ui->options_groupBox->setVisible(true);
    ui->advanced_groupBox->setVisible(true);

    populating = true;

    ui->solver_label->setText(sc->solverDefinition.name + " " + sc->solverDefinition.version);

    ui->makeConfigDefault_pushButton->setEnabled(sc->isBuiltin && !sc->solverDefinition.isDefaultSolver);

    ui->reset_pushButton->setEnabled(sc->isBuiltin);

    if (overrideSync || !ui->sync_checkBox->isChecked()) {
        ui->timeLimit_doubleSpinBox->setValue(sc->timeLimit / 1000.0);
        ui->timeLimit_checkBox->setChecked(sc->timeLimit != 0);

        if (sc->printIntermediate && sc->numSolutions == 1) {
            ui->defaultBehavior_radioButton->setChecked(true);
        } else {
            ui->userDefinedBehavior_radioButton->setChecked(true);
        }

        ui->intermediateSolutions_checkBox->setChecked(sc->printIntermediate);

        ui->numSolutions_spinBox->setValue(sc->numSolutions);
        ui->numSolutions_checkBox->setChecked(sc->numSolutions != 0);

        ui->numOptimal_spinBox->setValue(sc->numOptimal);
        ui->numOptimal_checkBox->setChecked(sc->numOptimal != 0);

        ui->verboseCompilation_checkBox->setChecked(sc->verboseCompilation);
        ui->verboseSolving_checkBox->setChecked(sc->verboseSolving);

        ui->compilationStats_checkBox->setChecked(sc->compilationStats);
        ui->solvingStats_checkBox->setChecked(sc->solvingStats);
        ui->timingInfo_checkBox->setChecked(sc->outputTiming);
    }

    ui->intermediateSolutions_checkBox->setEnabled(
                sc->solverDefinition.stdFlags.contains("-a") ||
                sc->solverDefinition.stdFlags.contains("-i"));

    ui->numSolutions_checkBox->setEnabled(sc->solverDefinition.stdFlags.contains("-a"));
    ui->numSolutions_spinBox->setEnabled(sc->solverDefinition.stdFlags.contains("-n"));

    ui->numOptimal_checkBox->setEnabled(sc->solverDefinition.stdFlags.contains("-a-o"));
    ui->numOptimal_spinBox->setEnabled(sc->solverDefinition.stdFlags.contains("-n-o"));

    ui->verboseSolving_checkBox->setEnabled(sc->solverDefinition.stdFlags.contains("-v"));

    ui->solvingStats_checkBox->setEnabled(sc->solverDefinition.stdFlags.contains("-s"));

    ui->optimizationLevel_comboBox->setCurrentIndex(sc->optimizationLevel);
    ui->additionalData_plainTextEdit->setPlainText(sc->additionalData.join("\n"));

    ui->numThreads_spinBox->setEnabled(sc->solverDefinition.stdFlags.contains("-p"));
    ui->numThreads_spinBox->setValue(sc->numThreads);

    bool enableRandomSeed = sc->solverDefinition.stdFlags.contains("-r");
    ui->randomSeed_lineEdit->setText(sc->randomSeed.toString());
    ui->randomSeed_checkBox->setChecked(!sc->randomSeed.isNull());
    ui->randomSeed_checkBox->setEnabled(enableRandomSeed);
    ui->randomSeed_lineEdit->setEnabled(enableRandomSeed && !sc->randomSeed.isNull());

    bool enableFreeSearch = sc->solverDefinition.stdFlags.contains("-f");
    ui->freeSearch_checkBox->setEnabled(enableFreeSearch);
    ui->freeSearch_checkBox->setChecked(sc->freeSearch);

    extraFlagsMenu->clear();
    for (auto& f : sc->solverDefinition.extraFlags) {
        auto action = extraFlagsMenu->addAction(f.description);
        action->setData(QVariant::fromValue(f));
        connect(action, &QAction::triggered, [=] (bool) {
            addExtraParam(f);
            action->setDisabled(true);
        });
    }
    extraFlagsMenu->addSeparator();
    extraFlagsMenu->addAction(ui->actionAdd_all_known_parameters);
    extraFlagsMenu->addAction(ui->actionCustom_Parameter);

    while (ui->extraParams_tableWidget->rowCount() > 0) {
        ui->extraParams_tableWidget->removeRow(0);
    }

    for (auto it = sc->extraOptions.begin(); it != sc->extraOptions.end(); it++) {
        bool matched = false;
        for (auto& f : sc->solverDefinition.extraFlags) {
            if (f.name == it.key()) {
                addExtraParam(f, it.value());
                matched = true;
                break;
            }
        }
        if (!matched) {
            addExtraParam(it.key(), it.value());
        }
    }

    resizeExtraFlagsTable();

    populating = false;
}


void ConfigWindow::updateSolverConfig(SolverConfiguration* sc)
{
    if (!sc->modified) {
        return;
    }

    auto cfgs = ui->sync_checkBox->isChecked() ? configs : (QList<SolverConfiguration*>() << sc);
    for (auto s : cfgs) {
        s->timeLimit = ui->timeLimit_checkBox->isChecked() ? static_cast<int>(ui->timeLimit_doubleSpinBox->value() * 1000.0) : 0;
        if (ui->defaultBehavior_radioButton->isChecked()) {
            s->printIntermediate = true;
            s->numSolutions = 1;
            s->numOptimal = 1;
        } else {
            s->printIntermediate = ui->intermediateSolutions_checkBox->isChecked();
            s->numSolutions = ui->numSolutions_checkBox->isChecked() ? ui->numSolutions_spinBox->value() : 0;
            s->numOptimal = ui->numOptimal_checkBox->isChecked() ? ui->numOptimal_spinBox->value() : 0;
        }

        s->verboseCompilation = ui->verboseCompilation_checkBox->isChecked();
        s->verboseSolving = ui->verboseSolving_checkBox->isChecked();
        s->compilationStats = ui->compilationStats_checkBox->isChecked();
        s->solvingStats = ui->solvingStats_checkBox->isChecked();
        s->outputTiming = ui->timingInfo_checkBox->isChecked();
    }

    sc->optimizationLevel = ui->optimizationLevel_comboBox->currentIndex();
    QString additionalData = ui->additionalData_plainTextEdit->toPlainText();
    sc->additionalData = additionalData.length() > 0 ? additionalData.split("\n") : QStringList();
    sc->numThreads = ui->numThreads_spinBox->value();
    sc->randomSeed = ui->randomSeed_checkBox->isChecked() ? ui->randomSeed_lineEdit->text() : nullptr;
    sc->freeSearch = ui->freeSearch_checkBox->isChecked();

    sc->extraOptions.clear();
    for (int row = 0; row < ui->extraParams_tableWidget->rowCount(); row++) {
        auto keyItem = ui->extraParams_tableWidget->item(row, 0);
        auto valueItem = ui->extraParams_tableWidget->item(row, 2);
        if (keyItem && valueItem) {
            auto key = keyItem->data(Qt::UserRole).isNull() ?
                        keyItem->data(Qt::DisplayRole).toString() :
                        keyItem->data(Qt::UserRole).toString();
            auto value = valueItem->data(Qt::DisplayRole);
            sc->extraOptions.insert(key, value);
        }
    }
}

void ConfigWindow::on_removeExtraParam_toolButton_clicked()
{
    QVector<int> toBeRemoved;
    for (auto& range : ui->extraParams_tableWidget->selectedRanges()) {
        for (int i = range.topRow(); i <= range.bottomRow(); i++) {
            toBeRemoved.append(i);
        }
    }
    std::sort(toBeRemoved.begin(), toBeRemoved.end(), std::greater<int>());
    for (auto i : toBeRemoved) {
        auto name = ui->extraParams_tableWidget->item(i, 0)->data(Qt::UserRole).toString();
        for (auto action : extraFlagsMenu->actions()) {
            // Re-enable adding of this flag
            if (name == qvariant_cast<SolverFlag>(action->data()).name) {
                action->setEnabled(true);
                break;
            }
        }
        ui->extraParams_tableWidget->removeRow(i);
    }
    if (!toBeRemoved.isEmpty()) {
        resizeExtraFlagsTable();
        invalidate(false);
    }
}

void ConfigWindow::on_extraParams_tableWidget_itemSelectionChanged()
{
    bool hasSelection = ui->extraParams_tableWidget->selectedRanges().length();
    ui->removeExtraParam_toolButton->setEnabled(hasSelection);
}

void ConfigWindow::addExtraParam(const SolverFlag& f, const QVariant& value)
{
    int i = ui->extraParams_tableWidget->rowCount();
    ui->extraParams_tableWidget->insertRow(i);

    auto keyItem = new QTableWidgetItem(f.description);
    keyItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    keyItem->setData(Qt::UserRole, f.name);

    auto typeItem = new QTableWidgetItem;
    typeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    auto valueItem = new QTableWidgetItem;
    valueItem->setData(Qt::UserRole, QVariant::fromValue(f));
    if (value.isNull()) {
        valueItem->setData(Qt::DisplayRole, f.def);
    } else {
        valueItem->setData(Qt::DisplayRole, value);
    }
    if (f.t == SolverFlag::T_BOOL && f.def.toBool()) {
        // Non switched booleans which are default true cannot be turned off
        // TODO: Maybe this should send the flag when different to default?
        valueItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    }

    ui->extraParams_tableWidget->setItem(i, 0, keyItem);
    ui->extraParams_tableWidget->setItem(i, 1, typeItem);
    ui->extraParams_tableWidget->setItem(i, 2, valueItem);

    resizeExtraFlagsTable();

    invalidate(false);
}

void ConfigWindow::addExtraParam(const QString& key, const QVariant& value)
{
    int i = ui->extraParams_tableWidget->rowCount();
    ui->extraParams_tableWidget->insertRow(i);

    auto flag = key.startsWith("--") ? key.right(key.length() - 2) : key;
    auto keyItem = new QTableWidgetItem(flag);
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

    connect(typeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [=] (int typeIndex) mutable {
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

    resizeExtraFlagsTable();
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
            connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), [=] (int) { action(); });
        } else if ((doubleSpinBox = qobject_cast<QDoubleSpinBox*>(widget))) {
            connect(doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=] (double) { action(); });
        } else if ((comboBox = qobject_cast<QComboBox*>(widget))) {
            connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [=] (int) { action(); });
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
    if (i >= 0 && i < configs.length() && !configs[i]->modified) {
        configs[i]->modified = true;
        refreshComboBox = true;
    }

    if (all && ui->sync_checkBox->isChecked()) {
        for (auto sc : configs) {
            // Only non-builtin configs really need to be saved after modifying basic options
            if (!sc->isBuiltin && !sc->modified) {
                sc->modified = true;
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
    for (auto sc : configs) {
        items << sc->name();
    }

    initialized = false;
    ui->config_comboBox->clear();
    ui->config_comboBox->addItems(items);
    setCurrentIndex(oldIndex);
    emit itemsChanged(items);
    initialized = oldInitialized;
}

void ConfigWindow::removeConfig(int i)
{
    bool targetIndex = i < currentIndex() ? currentIndex() - 1 : currentIndex();
    delete configs[i];
    configs.removeAt(i);
    populateComboBox();
    setCurrentIndex(targetIndex);
}

void ConfigWindow::on_clone_pushButton_clicked()
{
    auto sc = configs[currentIndex()];
    updateSolverConfig(sc);
    auto clone = new SolverConfiguration(*sc);
    clone->paramFile = "";
    clone->isBuiltin = false;
    clone->modified = true;
    configs << clone;
    populateComboBox();
    setCurrentIndex(configs.length() - 1);
}

void ConfigWindow::resizeExtraFlagsTable()
{
    int total_height = ui->extraParams_tableWidget->horizontalHeader()->height();
    if (!ui->extraParams_tableWidget->horizontalScrollBar()->isHidden()) {
        total_height += ui->extraParams_tableWidget->horizontalScrollBar()->height();
    }
    for (int row = 0; row < ui->extraParams_tableWidget->rowCount(); row++) {
        total_height += ui->extraParams_tableWidget->rowHeight(row);
    }
    ui->extraParams_tableWidget->setMinimumHeight(total_height);

}

QWidget* ExtraOptionDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (index.data(Qt::UserRole).canConvert<SolverFlag>()) {
        auto f = qvariant_cast<SolverFlag>(index.data(Qt::UserRole));
        switch (f.t) {
        case SolverFlag::T_INT:
        {
            auto field = new QLineEdit(parent);
            field->setAlignment(Qt::AlignRight);
            field->setValidator(new QIntValidator());
            return field;
        }
        case SolverFlag::T_INT_RANGE:
        {
            auto field = new QSpinBox(parent);
            field->setRange(static_cast<int>(f.min), static_cast<int>(f.max));
            return field;
        }
        case SolverFlag::T_FLOAT:
        {
            auto field = new QLineEdit(parent);
            field->setAlignment(Qt::AlignRight);
            field->setValidator(new QDoubleValidator());
            return field;
        }
        case SolverFlag::T_FLOAT_RANGE:
        {
            auto field = new QDoubleSpinBox(parent);
            field->setRange(f.min, f.max);
            return field;
        }
        case SolverFlag::T_OPT:
        {
            auto field = new QComboBox(parent);
            field->addItems(f.options);
            return field;
        }
        case SolverFlag::T_SOLVER:
        {
            auto field = new QComboBox(parent);
            for (auto& solver : MznDriver::get().solvers()) {
                field->addItem(solver.name);
            }
            return field;
        }
        case SolverFlag::T_BOOL:
        case SolverFlag::T_BOOL_ONOFF:
        case SolverFlag::T_STRING:
            return QStyledItemDelegate::createEditor(parent, option, index);
        }
    } else {
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

void ExtraOptionDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    if (index.data(Qt::UserRole).canConvert<SolverFlag>()) {
        auto f = qvariant_cast<SolverFlag>(index.data(Qt::UserRole));
        switch (f.t) {
        case SolverFlag::T_INT:
            static_cast<QLineEdit*>(editor)->setText(QString::number(index.data().toInt()));
            break;
        case SolverFlag::T_INT_RANGE:
            static_cast<QSpinBox*>(editor)->setValue(index.data().toInt());
            break;
        case SolverFlag::T_FLOAT:
            static_cast<QLineEdit*>(editor)->setText(QString::number(index.data().toDouble()));
            break;
        case SolverFlag::T_FLOAT_RANGE:
            static_cast<QDoubleSpinBox*>(editor)->setValue(index.data().toDouble());
            break;
        case SolverFlag::T_OPT:
        case SolverFlag::T_SOLVER:
            static_cast<QComboBox*>(editor)->setCurrentText(index.data().toString());
            break;
        case SolverFlag::T_BOOL:
        case SolverFlag::T_BOOL_ONOFF:
        case SolverFlag::T_STRING:
            QStyledItemDelegate::setEditorData(editor, index);
            break;
        }
    } else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void ExtraOptionDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    if (index.data(Qt::UserRole).canConvert<SolverFlag>()) {
        auto f = qvariant_cast<SolverFlag>(index.data(Qt::UserRole));
        switch (f.t) {
        case SolverFlag::T_INT:
            model->setData(index, static_cast<QLineEdit*>(editor)->text().toInt());
            break;
        case SolverFlag::T_INT_RANGE:
            model->setData(index, static_cast<QSpinBox*>(editor)->value());
            break;
        case SolverFlag::T_BOOL:
        case SolverFlag::T_BOOL_ONOFF:
            QStyledItemDelegate::setModelData(editor, model, index);
            break;
        case SolverFlag::T_FLOAT:
            model->setData(index, static_cast<QLineEdit*>(editor)->text().toDouble());
            break;
        case SolverFlag::T_FLOAT_RANGE:
            model->setData(index, static_cast<QDoubleSpinBox*>(editor)->value());
            break;
        case SolverFlag::T_STRING:
            model->setData(index, static_cast<QLineEdit*>(editor)->text());
            break;
        case SolverFlag::T_OPT:
        case SolverFlag::T_SOLVER:
            model->setData(index, static_cast<QComboBox*>(editor)->currentText());
            break;
        }
    } else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

void ConfigWindow::on_makeConfigDefault_pushButton_clicked()
{
    auto sc = currentSolverConfig();
    if (!sc) {
        return;
    }
    try {
        MznDriver::get().setDefaultSolver(sc->solverDefinition);
        ui->makeConfigDefault_pushButton->setDisabled(true);
    } catch (Exception& e) {
        QMessageBox::warning(this, "MiniZinc IDE", e.message(), QMessageBox::Ok);
    }
}

void ConfigWindow::on_actionAdd_all_known_parameters_triggered()
{
    for (auto action : extraFlagsMenu->actions()) {
        if (!action->isEnabled() ||
                action == ui->actionCustom_Parameter ||
                action == ui->actionAdd_all_known_parameters) {
            continue;
        }
        action->trigger();
    }
}

void ConfigWindow::on_reset_pushButton_clicked()
{
    auto sc = currentSolverConfig();
    if (!sc || !sc->isBuiltin) {
        return;
    }

    auto* newSc = new SolverConfiguration(sc->solverDefinition, true);
    int i = currentIndex();
    delete configs[i];
    configs.replace(i, newSc);
    updateGUI(true);
    populateComboBox();
}
