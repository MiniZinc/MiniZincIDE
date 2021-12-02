#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QTemporaryFile>
#include <QListWidgetItem>
#include "codeeditor.h"

namespace Ui {
class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(bool addNewSolver, QWidget *parent = nullptr);
    ~PreferencesDialog();

    void accept() Q_DECL_OVERRIDE;

private slots:
    void on_fontComboBox_currentFontChanged(const QFont &f);

    void on_fontSize_spinBox_valueChanged(int arg1);

    void on_lineWrapping_checkBox_stateChanged(int arg1);

    void on_theme_comboBox_currentIndexChanged(int index);

    void on_solvers_combo_currentIndexChanged(int index);

    void on_tabWidget_currentChanged(int index);

    void on_PreferencesDialog_rejected();

    void on_deleteButton_clicked();

    void on_mznpath_select_clicked();

    void on_exec_select_clicked();

    void on_PreferencesDialog_accepted();

    void on_mznDistribPath_returnPressed();

    void on_check_solver_clicked();

    void on_mznlib_select_clicked();

    void on_extraSearchPathAdd_pushButton_clicked();

    void on_extraSearchPathEdit_pushButton_clicked();

    void on_extraSearchPathDelete_pushButton_clicked();

    void on_extraSearchPath_listWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    void updateSolverLabel();

    void on_darkMode_checkBox_stateChanged(int arg1);

private:
    Ui::PreferencesDialog *ui;

    CodeEditor* _ce = nullptr;
    bool _solversPopulated = false;
    int _editingSolverIndex = -1;
    bool _extraSearchPathsChanged = false;
    QMap<QString, QByteArray> _restore;
    QSet<QString> _remove;
    QString _origMznDistribPath;

    bool _origDarkMode = false;
    int _origThemeIndex = 0;

    QByteArray allowFileRestore(const QString& path);
    bool loadDriver(bool showError);
    void populateSolvers();
    bool updateSolver();
    void updateSearchPaths();
    void showMessageBox(const QString& message);
};

#endif // PREFERENCESDIALOG_H
