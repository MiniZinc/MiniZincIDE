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
#include <QValidator>
#include <QWidgetAction>

#include "process.h"
#include "exception.h"
#include "ideutils.h"

#include <algorithm>
#include <limits>
#include <cfloat>

ConfigWindow::ConfigWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigWindow)
{
    ui->setupUi(this);
    ui->userDefinedBehavior_frame->setVisible(false);

    extraParamDialog = new ExtraParamDialog;

    auto* widgetAction = new QWidgetAction(nullptr);
    widgetAction->setDefaultWidget(extraParamDialog);

    auto* menu = new QMenu(this);
    menu->addAction(widgetAction);
    ui->addExtraParam_toolButton->setMenu(menu);

    connect(extraParamDialog, &ExtraParamDialog::addParams, this, [=](const QList<SolverFlag>& flags) {
        for (auto& f : flags) {
            addExtraParam(f);
            extraParamDialog->setParamEnabled(f, false);
        }
        resizeExtraFlagsTable();
        invalidate(false);
        menu->hide();
    });

    connect(extraParamDialog, &ExtraParamDialog::addCustomParam, this, [=]() {
        addExtraParam();
        resizeExtraFlagsTable();
        invalidate(false);
        menu->hide();
    });

    ui->extraParams_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->extraParams_tableWidget->setItemDelegateForColumn(3, new ExtraOptionDelegate);

    IDEUtils::watchChildChanges(ui->options_groupBox, this, std::bind(&ConfigWindow::invalidate, this, true));
    IDEUtils::watchChildChanges(ui->advanced_groupBox, this, std::bind(&ConfigWindow::invalidate, this, false));
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
                populating = true; // Make sure this doesn't mark the config as modified
                ui->sync_checkBox->setChecked(false);
                populating = false;
                break;
            }
        }
        populateComboBox();

        if (ui->sync_checkBox->isChecked()) {
            // Ensure that newly loaded config overrides synced options
            populating = true; // Make sure this doesn't mark the config as modified
            ui->sync_checkBox->setChecked(false);
            populating = false;
            setCurrentIndex(configs.length() - 1);
            populating = true; // Make sure this doesn't mark the config as modified
            ui->sync_checkBox->setChecked(true);
            populating = false;
        }

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
    if (file.isEmpty()) {
        // Unsaved configuration
        return -1;
    }
    int i = 0;
    for (auto sc : configs) {
        if (sc->paramFile == file) {
            return i;
        }
        i++;
    }
    return -1;
}

void ConfigWindow::stashModifiedConfigs()
{
    auto* selected = currentSolverConfig();
    for (auto* sc : configs) {
        if (sc->modified) {
            if (sc == selected) {
                stashSelectedModifiedConfig = stash.length();
            }
            stash.append(StashItem(sc));
        }
    }
    if (selected != nullptr) {
        if (selected->isBuiltin) {
            stashSelectedBuiltinSolverId = selected->solverDefinition.id;
            stashSelectedBuiltinSolverVersion = selected->solverDefinition.version;
        }
        stashSelectedParamFile = selected->paramFile;
    }
}

void ConfigWindow::unstashModifiedConfigs()
{
    lastIndex = -1;

    QList<SolverConfiguration*> merge;
    for (auto& item : stash) {
        merge.append(item.consume());
    }
    mergeConfigs(merge);

    int i = -1;
    if (stashSelectedModifiedConfig != -1) {
        i = configs.indexOf(merge[stashSelectedModifiedConfig]);
    } else if (!stashSelectedParamFile.isEmpty()) {
        i = findConfigFile(stashSelectedParamFile);
    } else if (!stashSelectedBuiltinSolverId.isEmpty()) {
        i = findBuiltinConfig(stashSelectedBuiltinSolverId, stashSelectedBuiltinSolverVersion);
    }
    if (i != -1) {
        setCurrentIndex(i);
    }

    stash.clear();
    stashSelectedBuiltinSolverId = "";
    stashSelectedBuiltinSolverVersion = "";
    stashSelectedParamFile = "";
    stashSelectedModifiedConfig = -1;
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

    ui->intermediateSolutions_checkBox->setEnabled(sc->supports("-a") || sc->supports("-i"));

    ui->numSolutions_checkBox->setEnabled(sc->supports("-a"));
    ui->numOptimal_checkBox->setEnabled(sc->supports("-a-o"));
    ui->verboseSolving_checkBox->setEnabled(sc->supports("-v"));
    ui->solvingStats_checkBox->setEnabled(sc->supports("-s"));

    if (overrideSync || !ui->sync_checkBox->isChecked()) {
        ui->timeLimit_checkBox->setChecked(sc->timeLimit != 0);
        ui->timeLimit_doubleSpinBox->setValue(sc->timeLimit / 1000.0);
        ui->timeLimit_doubleSpinBox->setEnabled(ui->timeLimit_checkBox->isChecked());

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

        if (ui->verboseSolving_checkBox->isEnabled()) {
            ui->verboseSolving_checkBox->setChecked(sc->verboseSolving);
        }
        ui->compilationStats_checkBox->setChecked(sc->compilationStats);

        if (ui->solvingStats_checkBox->isEnabled()) {
            ui->solvingStats_checkBox->setChecked(sc->solvingStats);
        }

        ui->timingInfo_checkBox->setChecked(sc->outputTiming);
    }

    ui->numSolutions_spinBox->setEnabled(ui->numSolutions_checkBox->isEnabled() && ui->numSolutions_checkBox->isChecked());
    ui->numOptimal_spinBox->setEnabled(ui->numOptimal_checkBox->isEnabled() && ui->numOptimal_checkBox->isChecked());

    ui->optimizationLevel_comboBox->setCurrentIndex(sc->optimizationLevel);
    ui->additionalData_plainTextEdit->setPlainText(sc->additionalData.join("\n"));

    ui->numThreads_spinBox->setEnabled(sc->supports("-p"));
    ui->numThreads_spinBox->setValue(sc->numThreads);

    bool enableRandomSeed = sc->supports("-r");
    ui->randomSeed_lineEdit->setText(sc->randomSeed.toString());
    ui->randomSeed_checkBox->setChecked(!sc->randomSeed.isNull());
    ui->randomSeed_checkBox->setEnabled(enableRandomSeed);
    ui->randomSeed_lineEdit->setEnabled(enableRandomSeed && !sc->randomSeed.isNull());

    bool enableFreeSearch = sc->supports("-f");
    ui->freeSearch_checkBox->setEnabled(enableFreeSearch);
    ui->freeSearch_checkBox->setChecked(sc->freeSearch);

    extraParamDialog->setParams(sc->solverDefinition.extraFlags);

    while (ui->extraParams_tableWidget->rowCount() > 0) {
        ui->extraParams_tableWidget->removeRow(0);
    }

    for (auto it = sc->extraOptions.begin(); it != sc->extraOptions.end(); it++) {
        bool matched = false;
        for (auto& f : sc->solverDefinition.extraFlags) {
            if (f.name == it.key()) {
                extraParamDialog->setParamEnabled(f, false);
                addExtraParam(f, it.value());
                matched = true;
                break;
            }
        }
        if (!matched) {
            addExtraParam(it.key(), false, it.value());
        }
    }

    for (auto it = sc->solverBackendOptions.begin(); it != sc->solverBackendOptions.end(); it++) {
        addExtraParam(it.key(), true, it.value());
    }

    resizeExtraFlagsTable();

    populating = false;
}


void ConfigWindow::updateSolverConfig(SolverConfiguration* sc)
{
    // Make sure if anything was being edited, we defocus to update the value
    auto* focus = focusWidget();
    if (focus && focus->hasFocus()) {
        focus->clearFocus();
        focus->setFocus();
    }

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
    sc->randomSeed = ui->randomSeed_checkBox->isChecked() ? ui->randomSeed_lineEdit->text() : QVariant();
    sc->freeSearch = ui->freeSearch_checkBox->isChecked();

    sc->extraOptions.clear();
    for (int row = 0; row < ui->extraParams_tableWidget->rowCount(); row++) {
        auto keyItem = ui->extraParams_tableWidget->item(row, 0);
        auto flagTypeWidget = static_cast<QComboBox*>(ui->extraParams_tableWidget->cellWidget(row, 1));
        auto valueItem = ui->extraParams_tableWidget->item(row, 3);
        if (keyItem && valueItem) {
            auto key = keyItem->data(Qt::UserRole).isNull() ?
                        keyItem->data(Qt::DisplayRole).toString() :
                        qvariant_cast<SolverFlag>(keyItem->data(Qt::UserRole)).name;
            auto value = valueItem->data(Qt::DisplayRole);
            if (flagTypeWidget && flagTypeWidget->currentIndex() == 1) {
                sc->solverBackendOptions.insert(key, value);
            } else {
                sc->extraOptions.insert(key, value);
            }
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
        auto data = ui->extraParams_tableWidget->item(i, 0)->data(Qt::UserRole);
        if (!data.isNull()) {
            // Re-enable adding of this flag
            extraParamDialog->setParamEnabled(qvariant_cast<SolverFlag>(data), true);
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
    keyItem->setData(Qt::UserRole, QVariant::fromValue(f));
    keyItem->setToolTip(f.name);

    auto flagTypeItem = new QTableWidgetItem;
    flagTypeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

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
    ui->extraParams_tableWidget->setItem(i, 1, flagTypeItem);
    ui->extraParams_tableWidget->setItem(i, 2, typeItem);
    ui->extraParams_tableWidget->setItem(i, 3, valueItem);

    invalidate(false);
}

void ConfigWindow::addExtraParam(const QString& key, bool backend, const QVariant& value)
{
    int i = ui->extraParams_tableWidget->rowCount();
    ui->extraParams_tableWidget->insertRow(i);

    auto flag = key.startsWith("--") ? key.right(key.length() - 2) : key;
    auto keyItem = new QTableWidgetItem(flag);
    auto valItem = new QTableWidgetItem;
    valItem->setData(Qt::DisplayRole, value);
    ui->extraParams_tableWidget->setItem(i, 0, keyItem);
    ui->extraParams_tableWidget->setItem(i, 3, valItem);

    auto flagTypeComboBox = new QComboBox;
    flagTypeComboBox->addItems({"MiniZinc", "Solver backend"});
    flagTypeComboBox->setCurrentIndex(backend ? 1 : 0);
    flagTypeComboBox->setToolTip("Controls whether the flag is sent to the underlying solver executable (if available), or to the minizinc command.");
    ui->extraParams_tableWidget->setCellWidget(i, 1, flagTypeComboBox);

    auto typeComboBox = new QComboBox;
    typeComboBox->setToolTip("Controls the data type of the flag value");
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
        ui->extraParams_tableWidget->setItem(valItem->row(), 3, newValItem);
        valItem = newValItem;
    });

    ui->extraParams_tableWidget->setCellWidget(i, 2, typeComboBox);
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
    if (oldIndex >= 0 && oldIndex < configs.length()) {
        setCurrentIndex(oldIndex);
    } else {
        // Index no longer valid, just go back to default solver
        int i = 0;
        for (auto config : configs) {
            if (config->solverDefinition.isDefaultSolver) {
                setCurrentIndex(i);
                break;
            }
            i++;
        }
    }
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
    auto* hScroll = ui->extraParams_tableWidget->horizontalScrollBar();
    // To avoid creating a vertical scrollbar if there's a horizontal one
    int padding = hScroll ? hScroll->height() : 0;
    int total_height = ui->extraParams_tableWidget->horizontalHeader()->height() + padding;
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
            field->setValidator(new LongLongValidator());
            return field;
        }
        case SolverFlag::T_INT_RANGE:
        {
            if (f.min_ll < std::numeric_limits<int>::min() || f.max_ll > std::numeric_limits<int>::max()) {
                auto field = new QLineEdit(parent);
                field->setAlignment(Qt::AlignRight);
                field->setValidator(new LongLongValidator(f.min_ll, f.max_ll));
                return field;
            } else {
                auto field = new QSpinBox(parent);
                field->setRange(static_cast<int>(f.min_ll), static_cast<int>(f.max_ll));
                return field;
            }
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
            field->setDecimals(DBL_MAX_10_EXP + DBL_DIG);
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
    } else if (index.data().type() == QVariant::Double) {
        auto field = new QLineEdit(parent);
        field->setAlignment(Qt::AlignRight);
        field->setValidator(new QDoubleValidator());
        return field;
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
            static_cast<QLineEdit*>(editor)->setText(QString::number(index.data().toLongLong()));
            break;
        case SolverFlag::T_INT_RANGE: {
            auto* e = qobject_cast<QSpinBox*>(editor);
            if (e) {
                e->setValue(index.data().toLongLong());
            } else {
                static_cast<QLineEdit*>(editor)->setText(QString::number(index.data().toLongLong()));
            }
            break;
        }
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
    } else if (index.data().type() == QVariant::Double) {
        static_cast<QLineEdit*>(editor)->setText(QString::number(index.data().toDouble()));
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
        case SolverFlag::T_INT_RANGE: {
            auto* e = qobject_cast<QSpinBox*>(editor);
            if (e) {
                model->setData(index, e->value());
            } else {
                model->setData(index, static_cast<QLineEdit*>(editor)->text().toLongLong());
            }
            break;
        }
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
    } else if (index.data().type() == QVariant::Double) {
        model->setData(index, static_cast<QLineEdit*>(editor)->text().toDouble());
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

ConfigWindow::StashItem::StashItem(SolverConfiguration* sc) :
    json(sc->toJSONObject()),
    isBuiltIn(sc->isBuiltin),
    paramFile(sc->paramFile)
{}

SolverConfiguration* ConfigWindow::StashItem::consume() {
    auto* sc = new (SolverConfiguration) (SolverConfiguration::loadJSON(QJsonDocument(json)));
    sc->isBuiltin = isBuiltIn;
    sc->paramFile = paramFile;
    return sc;
}
