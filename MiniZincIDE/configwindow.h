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
#include <QFormLayout>
#include <QMenu>
#include <QStyledItemDelegate>

#include "solverconfiguration.h"
#include "solverdialog.h"
#include "extraparamdialog.h"

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
    /// \return True if loading the config was successful
    ///
    bool addConfig(const QString& fileName);

    ///
    /// \brief Adds/merges in existing solver configurations
    /// \param configs The configurations to add
    ///
    void mergeConfigs(const QList<SolverConfiguration*> configs);

    int findBuiltinConfig(const QString& id, const QString& version);
    int findConfigFile(const QString& file);

    void removeConfig(int i);

    ///
    /// \brief Stash modified configs so they can be re-added later
    /// Stash modified configs and record the currently selected one
    ///
    void stashModifiedConfigs();

    ///
    /// \brief Unstash previously stashed modified configs
    /// Unstash modified configs and select the config that was active during stashing
    ///
    void unstashModifiedConfigs();

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
    void selectedSolverConfigurationChanged(const SolverConfiguration* sc);
    void itemsChanged(const QStringList& items);
    void configSaved(const QString& filename);

private slots:
    void on_numSolutions_checkBox_stateChanged(int arg1);

    void on_timeLimit_checkBox_stateChanged(int arg1);

    void on_randomSeed_checkBox_stateChanged(int arg1);

    void on_defaultBehavior_radioButton_toggled(bool checked);

    void on_userDefinedBehavior_radioButton_toggled(bool checked);

    void on_config_comboBox_currentIndexChanged(int index);

    void on_removeExtraParam_toolButton_clicked();

    void on_extraParams_tableWidget_itemSelectionChanged();

    void on_numOptimal_checkBox_stateChanged(int arg1);

    void on_clone_pushButton_clicked();

    void on_makeConfigDefault_pushButton_clicked();

    void on_reset_pushButton_clicked();

private:
    class StashItem {
    public:
        StashItem() {}
        StashItem(SolverConfiguration* sc);
        SolverConfiguration* consume();
    private:
        QJsonObject json;
        bool isBuiltIn;
        QString paramFile;
    };

    Ui::ConfigWindow *ui;

    QList<SolverConfiguration*> configs;

    QVector<StashItem> stash;
    QString stashSelectedBuiltinSolverId;
    QString stashSelectedBuiltinSolverVersion;
    QString stashSelectedParamFile;
    int stashSelectedModifiedConfig = -1;

    int lastIndex = -1;
    bool initialized = false;
    bool populating = false;

    void updateGUI(bool overrideSync = false);
    void updateSolverConfig(SolverConfiguration* sc);
    void addExtraParam(const QString& key = "", bool backend = false, const QVariant& value = "");
    void addExtraParam(const SolverFlag& f, const QVariant& value = QVariant());
    void watchChanges(const QList<QWidget*>& objects, std::function<void()> action);
    void invalidate(bool all);
    void populateComboBox(void);
    void resizeExtraFlagsTable(void);

    ExtraParamDialog* extraParamDialog;
};

class ExtraOptionDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
};

class LongLongValidator : public QValidator {
    Q_OBJECT
public:
    LongLongValidator(qlonglong min, qlonglong max, QObject *parent = nullptr)
        : QValidator(parent), _min(min), _max(max) {}
    LongLongValidator(QObject *parent = nullptr) : QValidator(parent) {}
    qlonglong bottom() const { return _min; }
    qlonglong top() const { return _max; }
    void setBottom(qlonglong min){
        if (_min != min) {
            _min = min;
            changed();
        }
    }
    void setTop(qlonglong max){
        if (_max != max) {
            _max = max;
            changed();
        }
    }
    void setRange(qlonglong min, qlonglong max){
        setBottom(min);
        setTop(max);
    }
    QValidator::State validate(QString &input, int&) const override{
        bool ok = false;
        qlonglong n = input.toLongLong(&ok);
        if (!ok) {
            return QValidator::Invalid;
        }
        if (n < _min || n > _max) {
            return QValidator::Intermediate;
        }
        return QValidator::Acceptable;
    }
private:
    qlonglong _min = std::numeric_limits<qlonglong>::min();
    qlonglong _max = std::numeric_limits<qlonglong>::max();
};



#endif // CONFIGWINDOW_H
