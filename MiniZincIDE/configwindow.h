/*
 *  Main authors:
 *     Jason Nguyen <jason.nguyen@monash.edu>
 */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CONFIGWINDOW_H
#define CONFIGWINDOW_H

#include <QWidget>
#include <QVariant>
#include <QJsonDocument>
#include <QStringListModel>

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
    /// \brief Saves the solver configuration with the given index
    /// \param index The index of the solver configuration to save
    /// \param saveAs Whether or not to always prompt for save location rather than overwrite existing
    /// \return The location of the saved file
    ///
    QString saveConfig(int index, bool saveAs = false);

    ///
    /// \brief Saves the currently solver configuration
    /// \param saveAs Whether or not to always prompt for save location rather than overwrite existing
    /// \return The location of the saved file
    ///
    QString saveConfig(bool saveAs = false);

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
    const QVector<SolverConfiguration>& solverConfigs(void);
signals:
    void selectedIndexChanged(int index);
    void itemsChanged(const QStringList& items);

private slots:
    void on_numSolutions_checkBox_stateChanged(int arg1);

    void on_timeLimit_checkBox_stateChanged(int arg1);

    void on_randomSeed_checkBox_stateChanged(int arg1);

    void on_defaultBehavior_radioButton_toggled(bool checked);

    void on_userDefinedBehavior_radioButton_toggled(bool checked);

    void on_config_comboBox_currentIndexChanged(int index);

    void on_addExtraParam_pushButton_clicked();

    void on_removeExtraParam_pushButton_clicked();

    void on_extraFlags_groupBox_toggled(bool arg1);

    void on_extraParams_tableWidget_itemSelectionChanged();

    void on_numOptimal_checkBox_stateChanged(int arg1);

private:
    Ui::ConfigWindow *ui;

    QVector<SolverConfiguration> configs;

    int lastIndex = -1;
    bool initialized = false;
    bool populating = false;

    QStringListModel* comboBoxModel;

    void updateSolverConfig(SolverConfiguration& sc);
    void addExtraParam(const QString& key, const QVariant& value);
    void watchChanges(const QList<QWidget*>& objects, std::function<void()> action);
    void invalidate(bool all);
    void populateComboBox(void);
};


#endif // CONFIGWINDOW_H
