/*
 *  Main authors:
 *     Jason Nguyen <jason.nguyen@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CONFIGWINDOW_H
#define CONFIGWINDOW_H

#include <functional>

#include <QWidget>
#include <QVariant>
#include <QJsonDocument>
#include <QStringListModel>
#include <QFormLayout>
#include <QMenu>
#include <QStyledItemDelegate>

#include "solverconfiguration.h"
#include "solverdialog.h"

namespace Ui {
class ConfigWindow;
}

class ConfigWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigWindow(QWidget *parent = nullptr);
    ~ConfigWindow();

    void init(void);

    ///
    /// \brief ConfigWindow::loadConfigs
    /// Loads or reloads solver configs, populating default solver configs, and reloading non-default configs
    ///
    void loadConfigs(void);

    ///
    /// \brief Loads a configuration from a JSON file
    /// \param fileName The path to the JSON file
    ///
    void addConfig(const QString& fileName);

    ///
    /// \brief Adds/merges in existing solver configurations
    /// \param configs The configurations to add
    ///
    void mergeConfigs(const QList<SolverConfiguration*> configs);

    int findBuiltinConfig(const QString& id, const QString& version);
    int findConfigFile(const QString& file);

    void removeConfig(int i);

    ///
    /// \brief Saves the solver configuration with the given index
    /// \param index The index of the solver configuration to save
    /// \return The location of the saved file or empty if cancelled
    ///
    QString saveConfig(int index);

    ///
    /// \brief Saves the currently solver configuration
    /// \return The location of the saved file or empty if cancelled
    ///
    QString saveConfig(void);

    ///
    /// \brief Gets the index of the current solver configuration
    /// \return The current solver configuration index
    ///
    int currentIndex(void);

    ///
    /// \brief Sets the current solver configuration index
    /// \param i The new solver configuration index
    ///
    void setCurrentIndex(int i);

    ///
    /// \brief Gets the active solver configuration
    /// \return The current solver config or nullptr
    ///
    SolverConfiguration* currentSolverConfig(void);

    ///
    /// \brief Gets a list of all loaded solver configurations
    /// \return A list of solver configurations
    ///
    const QList<SolverConfiguration*>& solverConfigs(void);
signals:
    void selectedIndexChanged(int index);
    void selectedSolverConfigurationChanged(const SolverConfiguration& sc);
    void itemsChanged(const QStringList& items);
    void configSaved(const QString& filename);

private slots:
    void on_numSolutions_checkBox_stateChanged(int arg1);

    void on_timeLimit_checkBox_stateChanged(int arg1);

    void on_randomSeed_checkBox_stateChanged(int arg1);

    void on_defaultBehavior_radioButton_toggled(bool checked);

    void on_userDefinedBehavior_radioButton_toggled(bool checked);

    void on_config_comboBox_currentIndexChanged(int index);

    void on_removeExtraParam_pushButton_clicked();

    void on_extraParams_tableWidget_itemSelectionChanged();

    void on_numOptimal_checkBox_stateChanged(int arg1);

    void on_clone_pushButton_clicked();

    void on_actionCustom_Parameter_triggered();

private:
    Ui::ConfigWindow *ui;

    QList<SolverConfiguration*> configs;

    int lastIndex = -1;
    bool initialized = false;
    bool populating = false;

    void updateSolverConfig(SolverConfiguration* sc);
    void addExtraParam(const QString& key = "", const QVariant& value = "");
    void addExtraParam(const SolverFlag& f, const QVariant& value = QVariant());
    void watchChanges(const QList<QWidget*>& objects, std::function<void()> action);
    void invalidate(bool all);
    void populateComboBox(void);
    void resizeExtraFlagsTable(void);

    QWidget* extraFlagsWidget = nullptr;
    QFormLayout* extraFlagsForm = nullptr;
    QMenu* extraFlagsMenu;
};

class ExtraOptionDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
};

#endif // CONFIGWINDOW_H
