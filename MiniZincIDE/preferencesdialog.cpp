#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

#include "exception.h"
#include "ideutils.h"
#include "mainwindow.h"

#include <QMessageBox>
#include <QSettings>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>

#ifdef Q_OS_WIN
#include "win_darkmode.h"
#elif Q_OS_MAC
#include "macos_extras.h"
#endif

PreferencesDialog::PreferencesDialog(bool addNewSolver, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    QSettings settings;

    // Get font so we can populate preview
    settings.beginGroup("MainWindow");
    QFont defaultFont;
    defaultFont.setFamily("Menlo");
    if (!defaultFont.exactMatch()) {
        defaultFont.setFamily("Consolas");
    }
    if (!defaultFont.exactMatch()) {
        defaultFont.setFamily("Courier New");
    }
    defaultFont.setStyleHint(QFont::TypeWriter);
    defaultFont.setPointSize(13);
    QFont editorFont;
    editorFont.fromString(settings.value("editorFont", defaultFont.toString()).value<QString>());
    _origDarkMode = settings.value("darkMode", false).toBool();
    settings.endGroup();

    if (_ce == nullptr) {
        // Populate preview
        QString fileContents;
        QFile file(":/cheat_sheet.mzn");
        if (file.open(QFile::ReadOnly)) {
            fileContents = file.readAll();
        } else {
            qDebug() << "internal error: cannot open cheat sheet.";
        }
        _ce = new CodeEditor(nullptr, ":/cheat_sheet.mzn", false, false, editorFont, true, nullptr, this);
        _ce->document()->setPlainText(fileContents);
        _ce->setReadOnly(true);
        ui->preview_verticalLayout->addWidget(_ce);
    }

    // Now everything else can get populated
    ui->fontComboBox->setCurrentFont(editorFont);
    ui->fontSize_spinBox->setValue(editorFont.pointSize());

    auto& driver = MznDriver::get();
    _origMznDistribPath = driver.mznDistribPath();
    ui->mznDistribPath->setText(_origMznDistribPath);
    ui->mzn2fzn_version->setText(driver.minizincVersionString());

    settings.beginGroup("ide");
    ui->check_updates->setChecked(settings.value("checkforupdates21",false).toBool());
    ui->checkSolutions_checkBox->setChecked(settings.value("checkSolutions", true).toBool());
    ui->clearOutput_checkBox->setChecked(settings.value("clearOutput", false).toBool());
    int compressSolutions = settings.value("compressSolutions", 100).toInt();
    if (compressSolutions > 0) {
        ui->compressSolutions_spinBox->setValue(compressSolutions);
        ui->compressSolutions_checkBox->setChecked(true);
    } else {
        ui->compressSolutions_checkBox->setChecked(false);
    }
    ui->reuseVis_checkBox->setChecked(settings.value("reuseVis", false).toBool());
    ui->printCommand_checkBox->setChecked(settings.value("printCommand", false).toBool());
    ui->indentSize_spinBox->setValue(settings.value("indentSize", 2).toInt());
    ui->lineWrapping_checkBox->setChecked(settings.value("wordWrap", true).toBool());
    _origThemeIndex = settings.value("theme", 0).toInt();
    ui->theme_comboBox->setCurrentIndex(_origThemeIndex);

#ifdef Q_OS_WIN
    DarkModeNotifier d;
    if (d.supportsDarkMode()) {
        _supportsDarkMode = true;
        _origDarkMode = d.darkMode();
        ui->darkMode_checkBox->hide();
    }
#elif Q_OS_MAC
    ui->darkModeAuto_radioButton->setDisabled(hasDarkMode());
    if (hasDarkMode()) {
        _supportsDarkMode = true;
        _origDarkMode = isDark();
        ui->darkMode_checkBox->hide();
    }
#endif
    _ce->setDarkMode(_origDarkMode);
    ui->darkMode_checkBox->setChecked(_origDarkMode);
    settings.endGroup();
    // Load user solver search paths
    auto& userConfigFile = driver.userConfigFile();
    QFile uc(userConfigFile);
    if (uc.exists() && uc.open(QFile::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(uc.readAll());
        auto obj = doc.object();
        if (obj.contains("mzn_solver_path")) {
            for (auto it : obj["mzn_solver_path"].toArray()) {
                auto path = it.toString();
                if (!path.isEmpty()) {
                    ui->extraSearchPath_listWidget->addItem(path);
                }
            }
        }
    }

    IDEUtils::watchChildChanges(ui->solverFrame, this, [=] () {
        _editingSolverIndex = ui->solvers_combo->currentIndex();
    });

    connect(ui->name, &QLineEdit::textChanged, this, &PreferencesDialog::updateSolverLabel);
    connect(ui->version, &QLineEdit::textChanged, this, &PreferencesDialog::updateSolverLabel);

    if (addNewSolver) {
        ui->tabWidget->setCurrentIndex(1);
        ui->solvers_combo->setCurrentIndex(ui->solvers_combo->count()-1);
    }
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
    delete _ce;
}

void PreferencesDialog::on_fontComboBox_currentFontChanged(const QFont &f)
{
    auto editorFont = f;
    _ce->setEditorFont(editorFont);
}


void PreferencesDialog::on_fontSize_spinBox_valueChanged(int size)
{
    auto editorFont = ui->fontComboBox->currentFont();
    editorFont.setPointSize(size);
    _ce->setEditorFont(editorFont);
}


void PreferencesDialog::on_lineWrapping_checkBox_stateChanged(int checkstate)
{
    bool checked = checkstate == Qt::Checked;
    if (checked) {
        _ce->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        _ce->setWordWrapMode(QTextOption::WrapAnywhere);
    } else {
        _ce->setLineWrapMode(QPlainTextEdit::NoWrap);
        _ce->setWordWrapMode(QTextOption::NoWrap);
    }
}


void PreferencesDialog::on_theme_comboBox_currentIndexChanged(int index)
{
    switch (index) {
    case 1:
        Themes::currentTheme = Themes::blueberry;
        break;
    case 2:
        Themes::currentTheme = Themes::mango;
        break;
    default:
        Themes::currentTheme = Themes::minizinc;
    }

    auto* mw = qobject_cast<MainWindow*>(parentWidget());
    if (mw != nullptr) {
        // Refresh style
        mw->setDarkMode(mw->isDarkMode());
    }

    _ce->setDarkMode(ui->darkMode_checkBox->isChecked());
}

QByteArray PreferencesDialog::allowFileRestore(const QString& path)
{
    QFileInfo fi(path);
    auto filePath = fi.canonicalFilePath();
    if (_restore.contains(filePath)) {
        // Already processed
        return _restore[filePath];
    }
    if (_remove.contains(filePath)) {
        // Already processed
        return QByteArray();
    }
    QFile f(path);
    if (f.exists()) {
        // Restore to old contents on cancel
        if (f.open(QIODevice::ReadOnly)) {
            auto contents = f.readAll();
            _restore[filePath] = contents;
            f.close();
            return contents;
        }
    } else {
        // Remove on cancel
        _remove << filePath;
    }
    return QByteArray();
}

bool PreferencesDialog::loadDriver(bool showError)
{
    auto& driver = MznDriver::get();
    auto& solvers = driver.solvers();

    _solversPopulated = false;
    auto dest = ui->solvers_combo->currentIndex();
    bool destExists = dest < solvers.size();
    QString matchId;
    QString matchVersion;
    if (dest == _editingSolverIndex) {
        // Was editing, so use widget values
        matchId = ui->solverId->text();
        matchVersion = ui->version->text();
        destExists = true;
    } else if (destExists) {
        // Switching to different solver
        matchId = solvers[dest].id;
        matchVersion = solvers[dest].version;
    }
    try {
        driver.setLocation(ui->mznDistribPath->text());
        ui->mzn2fzn_version->setText(driver.minizincVersionString());
    } catch (Exception& e) {
        if (showError) {
            showMessageBox(e.message());
        }
        ui->mzn2fzn_version->setText(e.message());
        return false;
    }

    populateSolvers();
    int index = 0;
    if (destExists) {
        int i = 0;
        for (auto& solver : solvers) {
            if (solver.id == matchId && solver.version == matchVersion) {
                index = i;
                break;
            }
            i++;
        }
    } else {
        index = ui->solvers_combo->count() - 1;
    }

    if (ui->solvers_combo->currentIndex() == index) {
        on_solvers_combo_currentIndexChanged(index);
    } else {
        ui->solvers_combo->setCurrentIndex(index);
    }

    return true;
}

void PreferencesDialog::populateSolvers()
{
    if (_solversPopulated) {
        return;
    }

    _editingSolverIndex = -1;

    auto& driver = MznDriver::get();
    auto& solvers = driver.solvers();

    while (ui->solvers_combo->count() > 0) {
        ui->solvers_combo->removeItem(0);
    }

    for (auto& solver : solvers) {
        ui->solvers_combo->addItem(solver.name + " " + solver.version);
    }
    ui->solvers_combo->addItem("Add new...");

    _solversPopulated = true;
}

bool PreferencesDialog::updateSolver()
{
    if (_editingSolverIndex == -1) {
        return true;
    }

    auto& driver = MznDriver::get();
    auto& solvers = driver.solvers();
    auto& userConfigFile = driver.userConfigFile();
    auto& userSolverConfigDir = driver.userSolverConfigDir();

    if (!ui->requiredFlags->isHidden()) {
        // This is an internal solver, we are only updating the required flags

        QGridLayout* rfLayout = static_cast<QGridLayout*>(ui->requiredFlags->layout());
        QVector<QStringList> defaultFlags;
        for (int row=0; row<rfLayout->rowCount(); row++) {
            if (QLayoutItem* li = rfLayout->itemAtPosition(row, 0)) {
                QString key = static_cast<QLabel*>(li->widget())->text();
                QLayoutItem* li2 = rfLayout->itemAtPosition(row, 1);
                QString val = static_cast<QLineEdit*>(li2->widget())->text();
                if (!val.isEmpty()) {
                    QStringList sl({ui->solverId->text(), key, val});
                    defaultFlags.push_back(sl);
                }
            }

        }

        auto doc = QJsonDocument::fromJson(allowFileRestore(userConfigFile));
        auto jo = doc.object();
        QJsonArray a0;
        if (!jo.contains("solverDefaults")) {
            // Fresh object, initialise
            for (auto& df : defaultFlags) {
                QJsonArray a1;
                for (auto& f : df) {
                    a1.append(f);
                }
                a0.append(a1);
            }
        } else {
            QJsonArray previous = jo["solverDefaults"].toArray();
            // First remove all entries for this solver
            for (auto triple : previous) {
                if (triple.isArray()) {
                    QJsonArray previousA1 = triple.toArray();
                    if (previousA1.size()==3) {
                        if (previousA1[0]!=ui->solverId->text()) {
                            a0.append(triple);
                        }
                    }
                }
            }
            // Now add the new values
            for (auto& df : defaultFlags) {
                QJsonArray a1;
                for (auto& f : df) {
                    a1.append(f);
                }
                a0.append(a1);
            }
        }
        jo["solverDefaults"] = a0;
        doc.setObject(jo);
        QFileInfo uc_info(userConfigFile);
        if (!QDir().mkpath(uc_info.absoluteDir().absolutePath())) {
            showMessageBox("Cannot create user configuration directory "+uc_info.absoluteDir().absolutePath());
            return false;
        }
        QFile uc(userConfigFile);
        if (uc.open(QFile::ReadWrite | QIODevice::Truncate)) {
            uc.write(doc.toJson());
            uc.close();
        } else {
            showMessageBox("Cannot write user configuration file "+userConfigFile);
            return false;
        }

    } else {

        if (ui->name->text().trimmed().isEmpty()) {
            showMessageBox("You need to specify a name for the solver.");
            return false;
        }
        if (ui->solverId->text().trimmed().isEmpty()) {
            showMessageBox("You need to specify a solver ID for the solver.");
            return false;
        }
        if (ui->executable->text().trimmed().isEmpty()) {
            showMessageBox("You need to specify an executable for the solver.");
            return false;
        }
        if (ui->version->text().trimmed().isEmpty()) {
            showMessageBox("You need to specify a version for the solver.");
            return false;
        }
        int index = _editingSolverIndex;
        for (int i=0; i<solvers.size(); i++) {
            if (i != index && ui->solverId->text().trimmed()==solvers[i].id) {
                showMessageBox("A solver with that solver ID already exists.");
                return false;
            }
        }
        if (index==solvers.size()) {
            Solver s;
            s.configFile = userSolverConfigDir+"/"+ui->solverId->text().trimmed()+".msc";
            if (!QDir().mkpath(userSolverConfigDir)) {
                showMessageBox("Cannot create user configuration directory "+userSolverConfigDir);
                return false;
            }
            solvers.append(s);
            allowFileRestore(s.configFile);
        }

        solvers[index].executable = ui->executable->text().trimmed();
        solvers[index].mznlib = ui->mznpath->text();
        solvers[index].name = ui->name->text().trimmed();
        solvers[index].id = ui->solverId->text().trimmed();
        solvers[index].version = ui->version->text().trimmed();
        solvers[index].isGUIApplication= ui->detach->isChecked();
        solvers[index].supportsMzn = !ui->needs_mzn2fzn->isChecked();
        solvers[index].needsSolns2Out = ui->needs_solns2out->isChecked();

        solvers[index].stdFlags.removeAll("-a");
        if (ui->has_stdflag_a->isChecked()) {
            solvers[index].stdFlags.push_back("-a");
        }
        solvers[index].stdFlags.removeAll("-n");
        if (ui->has_stdflag_n->isChecked()) {
            solvers[index].stdFlags.push_back("-n");
        }
        solvers[index].stdFlags.removeAll("-p");
        if (ui->has_stdflag_p->isChecked()) {
            solvers[index].stdFlags.push_back("-p");
        }
        solvers[index].stdFlags.removeAll("-s");
        if (ui->has_stdflag_s->isChecked()) {
            solvers[index].stdFlags.push_back("-s");
        }
        solvers[index].stdFlags.removeAll("-v");
        if (ui->has_stdflag_v->isChecked()) {
            solvers[index].stdFlags.push_back("-v");
        }
        solvers[index].stdFlags.removeAll("-r");
        if (ui->has_stdflag_r->isChecked()) {
            solvers[index].stdFlags.push_back("-r");
        }
        solvers[index].stdFlags.removeAll("-f");
        if (ui->has_stdflag_f->isChecked()) {
            solvers[index].stdFlags.push_back("-f");
        }
        solvers[index].stdFlags.removeAll("-t");
        if (ui->has_stdflag_t->isChecked()) {
            solvers[index].stdFlags.push_back("-t");
        }

        QJsonObject json = solvers[index].json;
        json.remove("extraInfo");
        json["executable"] = ui->executable->text().trimmed();
        json["mznlib"] = ui->mznpath->text();
        json["name"] = ui->name->text().trimmed();
        json["id"] = ui->solverId->text().trimmed();
        json["version"] = ui->version->text().trimmed();
        json["isGUIApplication"] = ui->detach->isChecked();
        json["supportsMzn"] = !ui->needs_mzn2fzn->isChecked();
        json["needsSolns2Out"] = ui->needs_solns2out->isChecked();
        json["stdFlags"] = QJsonArray::fromStringList(solvers[index].stdFlags);
        QJsonDocument jdoc(json);
        QFile jdocFile(solvers[index].configFile);
        allowFileRestore(solvers[index].configFile);
        if (!jdocFile.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
            showMessageBox("Cannot save configuration file "+solvers[index].configFile);
            return false;
        }
        if (jdocFile.write(jdoc.toJson())==-1) {
            showMessageBox("Cannot save configuration file "+solvers[index].configFile);
            return false;
        }
    }
    _editingSolverIndex = -1;
    loadDriver(false);
    return true;
}

namespace {

    void clearRequiredFlagsLayout(QGridLayout* l) {
        QVector<QLayoutItem*> items;
        for (int i=0; i<l->rowCount(); i++) {
            for (int j=0; j<l->columnCount(); j++) {
                if (QLayoutItem* li = l->itemAtPosition(i,j)) {
                    if (QWidget* w = li->widget()) {
                        w->hide();
                        w->deleteLater();
                    }
                    items.push_back(li);
                }
            }
        }
        for (auto i : items) {
            l->removeItem(i);
            delete i;
        }
    }

}

void PreferencesDialog::on_deleteButton_clicked()
{
    auto& driver = MznDriver::get();
    auto& solvers = driver.solvers();
    int index = ui->solvers_combo->currentIndex();
    if (index >= solvers.size()) {
        _editingSolverIndex = -1;
        ui->solvers_combo->removeItem(index);
        on_solvers_combo_currentIndexChanged(0);
        return;
    }
    if (QMessageBox::warning(this,
                                                       "MiniZinc IDE",
                                                       "Delete solver " + solvers[index].name + "?",
                                                       QMessageBox::Ok | QMessageBox::Cancel)
            == QMessageBox::Ok) {
        auto configFile = solvers[index].configFile;
        allowFileRestore(configFile);
        QFile sf(configFile);
        if (!sf.remove()) {
            showMessageBox("Cannot remove configuration file "+solvers[index].configFile);
            return;
        }
        solvers.remove(index);
        ui->solvers_combo->removeItem(index);
        _editingSolverIndex = -1;
        on_solvers_combo_currentIndexChanged(ui->solvers_combo->currentIndex());
    }
}

void PreferencesDialog::on_mznpath_select_clicked()
{
    QFileDialog fd(this,"Select MiniZinc distribution path (bin directory)");
    QDir dir(ui->mznDistribPath->text());
    fd.setDirectory(dir);
    fd.setFileMode(QFileDialog::Directory);
    fd.setOption(QFileDialog::ShowDirsOnly, true);
    if (fd.exec()) {
        ui->mznDistribPath->setText(fd.selectedFiles().first());
        loadDriver(true);
    }
}

void PreferencesDialog::on_exec_select_clicked()
{
    QFileDialog fd(this,"Select solver executable");
    fd.selectFile(ui->executable->text());
    fd.setFileMode(QFileDialog::ExistingFile);
    if (fd.exec()) {
        ui->executable->setText(fd.selectedFiles().first());
    }
    QFileInfo fi(ui->executable->text());
    ui->exeNotFoundLabel->setHidden(fi.exists());
}

void PreferencesDialog::on_mznlib_select_clicked()
{
    QFileDialog fd(this,"Select solver library path");
    QDir dir(ui->mznpath->text());
    fd.setDirectory(dir);
    fd.setFileMode(QFileDialog::Directory);
    fd.setOption(QFileDialog::ShowDirsOnly, true);
    if (fd.exec()) {
        ui->mznpath->setText(fd.selectedFiles().first());
    }
}

void PreferencesDialog::on_mznDistribPath_returnPressed()
{
    loadDriver(true);
}


void PreferencesDialog::on_check_solver_clicked()
{
    loadDriver(true);
}


void PreferencesDialog::on_extraSearchPathAdd_pushButton_clicked()
{
    QFileDialog fd(this ,"Select path containing solver configuration (.msc) files");
    auto selected = ui->extraSearchPath_listWidget->selectedItems();
    if (!selected.isEmpty()) {
        QDir dir(selected.first()->text());
        fd.setDirectory(dir);
    }
    fd.setFileMode(QFileDialog::Directory);
    fd.setOption(QFileDialog::ShowDirsOnly, true);
    if (fd.exec()) {
        for (auto& dir : fd.selectedFiles()) {
            ui->extraSearchPath_listWidget->addItem(dir);
        }
        _extraSearchPathsChanged = true;
    }
}

void PreferencesDialog::on_extraSearchPathEdit_pushButton_clicked()
{
    QFileDialog fd(this, "Select path containing solver configuration (.msc) files");
    auto selected = ui->extraSearchPath_listWidget->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    QDir dir(selected.first()->text());
    fd.setDirectory(dir);
    fd.setFileMode(QFileDialog::Directory);
    fd.setOption(QFileDialog::ShowDirsOnly, true);
    if (fd.exec()) {
        selected.first()->setText(fd.selectedFiles().first());
        _extraSearchPathsChanged = true;
    }
}

void PreferencesDialog::on_extraSearchPathDelete_pushButton_clicked()
{
    auto items = ui->extraSearchPath_listWidget->selectedItems();
    if (!items.isEmpty()) {
        delete items.first();
        _extraSearchPathsChanged = true;
    }
}

void PreferencesDialog::on_extraSearchPath_listWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    bool hasSelection = current != nullptr;
    ui->extraSearchPathEdit_pushButton->setEnabled(hasSelection);
    ui->extraSearchPathDelete_pushButton->setEnabled(hasSelection);
}

void PreferencesDialog::updateSearchPaths()
{
    if (!_extraSearchPathsChanged) {
        return;
    }

    auto& driver = MznDriver::get();
    auto& userConfigFile = driver.userConfigFile();
    QFile uc(userConfigFile);
    auto doc = QJsonDocument::fromJson(allowFileRestore(userConfigFile));
    auto obj = doc.object();
    auto* list = ui->extraSearchPath_listWidget;
    if (list->count() > 0) {
        QJsonArray arr;
        for (auto i = 0; i < list->count(); i++) {
            auto path = list->item(i)->text();
            if (!path.isEmpty()) {
                arr.append(path);
            }
        }
        obj["mzn_solver_path"] = arr;
    } else if (obj.contains("mzn_solver_path")) {
        obj.remove("mzn_solver_path");
    }
    if (uc.open(QFile::ReadWrite | QIODevice::Truncate)) {
        uc.write(QJsonDocument(obj).toJson());
        uc.close();
        _extraSearchPathsChanged = false;
        loadDriver(true);
    } else {
        showMessageBox("Cannot write user configuration file " + userConfigFile);
    }
}

void PreferencesDialog::on_solvers_combo_currentIndexChanged(int index)
{
    if (!_solversPopulated || index == _editingSolverIndex) {
        return;
    }
    if (_editingSolverIndex != -1) {
        if (!updateSolver()) {
            // Cannot edit another solver unless this one is valid
            ui->solvers_combo->setCurrentIndex(_editingSolverIndex);
        }
        return;
    }

    auto& driver = MznDriver::get();
    auto& solvers = driver.solvers();
    QGridLayout* rfLayout = static_cast<QGridLayout*>(ui->requiredFlags->layout());
    clearRequiredFlagsLayout(rfLayout);
    QString userSolverConfigCanonical = QFileInfo(driver.userSolverConfigDir()).canonicalPath();
    if (index<solvers.size()) {
        ui->name->setText(solvers[index].name);
        ui->solverId->setText(solvers[index].id);
        ui->version->setText(solvers[index].version);
        ui->executable->setText(solvers[index].executable);
        ui->exeNotFoundLabel->setVisible(!solvers[index].executable.isEmpty() && solvers[index].executable_resolved.isEmpty());
        ui->detach->setChecked(solvers[index].isGUIApplication);
        ui->needs_mzn2fzn->setChecked(!solvers[index].supportsMzn);
        ui->needs_solns2out->setChecked(solvers[index].needsSolns2Out);
        ui->mznpath->setText(solvers[index].mznlib);
        bool solverConfigIsUserEditable = false;
        if (!solvers[index].configFile.isEmpty()) {
            QFileInfo configFileInfo(solvers[index].configFile);
            if (configFileInfo.canonicalPath().startsWith(userSolverConfigCanonical)) {
                solverConfigIsUserEditable = true;
            }
        }

        ui->deleteButton->setEnabled(solverConfigIsUserEditable);
        ui->solverFrame->setEnabled(solverConfigIsUserEditable);

        ui->has_stdflag_a->setChecked(solvers[index].stdFlags.contains("-a"));
        ui->has_stdflag_p->setChecked(solvers[index].stdFlags.contains("-p"));
        ui->has_stdflag_r->setChecked(solvers[index].stdFlags.contains("-r"));
        ui->has_stdflag_n->setChecked(solvers[index].stdFlags.contains("-n"));
        ui->has_stdflag_s->setChecked(solvers[index].stdFlags.contains("-s"));
        ui->has_stdflag_f->setChecked(solvers[index].stdFlags.contains("-f"));
        ui->has_stdflag_v->setChecked(solvers[index].stdFlags.contains("-v"));
        ui->has_stdflag_t->setChecked(solvers[index].stdFlags.contains("-t"));

        if (solvers[index].requiredFlags.size()==0) {
            ui->requiredFlags->hide();
        } else {
            ui->requiredFlags->show();
            int row = 0;
            for (auto& rf : solvers[index].requiredFlags) {
                QString val;
                int foundFlag = solvers[index].defaultFlags.indexOf(rf);
                if (foundFlag != -1 && foundFlag < solvers[index].defaultFlags.size()-1) {
                    val = solvers[index].defaultFlags[foundFlag+1];
                }
                rfLayout->addWidget(new QLabel(rf), row, 0);
                rfLayout->addWidget(new QLineEdit(val), row, 1);
                row++;
            }
        }
        _editingSolverIndex = -1;
    } else {
        ui->solvers_combo->setItemText(index, "New solver");
        ui->solvers_combo->addItem("Add new...");

        ui->name->setText("");
        ui->solverId->setText("");
        ui->version->setText("");
        ui->executable->setText("");
        ui->detach->setChecked(false);
        ui->needs_mzn2fzn->setChecked(true);
        ui->needs_solns2out->setChecked(true);
        ui->mznpath->setText("");
        ui->solverFrame->setEnabled(true);
        ui->deleteButton->setEnabled(true);
        ui->has_stdflag_a->setChecked(false);
        ui->has_stdflag_p->setChecked(false);
        ui->has_stdflag_r->setChecked(false);
        ui->has_stdflag_n->setChecked(false);
        ui->has_stdflag_s->setChecked(false);
        ui->has_stdflag_v->setChecked(false);
        ui->has_stdflag_f->setChecked(false);
        ui->has_stdflag_t->setChecked(false);
        ui->requiredFlags->hide();

        _editingSolverIndex = index;
    }
}

void PreferencesDialog::showMessageBox(const QString& message)
{
    // Defer message box so that we can return immediately
    QTimer::singleShot(0, this, [=] () {
        QMessageBox::warning(this, "MiniZinc IDE", message, QMessageBox::Ok);
    });
}

void PreferencesDialog::on_tabWidget_currentChanged(int index)
{
    updateSearchPaths();
    if (index == 1 && !_solversPopulated) {
        populateSolvers();
        on_solvers_combo_currentIndexChanged(0);
    }
    // Cannot change out of solver tab if changes invalid
    if (index != 1 && !updateSolver()) {
        ui->tabWidget->setCurrentIndex(1);
    }
}


void PreferencesDialog::accept()
{
    if (ui->tabWidget->currentIndex() == 1 && !updateSolver()) {
        // Can't accept until updateSolver() succeeds
        return;
    }

    QDialog::accept();
}


void PreferencesDialog::on_PreferencesDialog_rejected()
{

    ui->theme_comboBox->setCurrentIndex(_origThemeIndex);
    ui->darkMode_checkBox->setChecked(_origDarkMode);

    if (_restore.empty() && _remove.empty()) {
        // Nothing to do
        return;
    }

    // Undo any file changes on cancellation
    for (auto it = _restore.begin(); it != _restore.end(); it++) {
        QFile f(it.key());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(it.value());
            f.close();
        }
    }
    for (auto& it : _remove) {
        QFile::remove(it);
    }
    _restore.clear();
    _remove.clear();

    // Reload old driver (may have updated things that have been reverted)
    try {
        MznDriver::get().setLocation(_origMznDistribPath);
    } catch (Exception& e) {}
}

void PreferencesDialog::on_PreferencesDialog_accepted()
{
    updateSearchPaths();

    QSettings settings;

    settings.beginGroup("ide");
    settings.setValue("checkforupdates21", ui->check_updates->isChecked());
    settings.setValue("checkSolutions", ui->checkSolutions_checkBox->isChecked());
    settings.setValue("clearOutput", ui->clearOutput_checkBox->isChecked());
    settings.setValue("compressSolutions", ui->compressSolutions_checkBox->isChecked()
                      ? ui->compressSolutions_spinBox->value() : 0);
    settings.setValue("printCommand", ui->printCommand_checkBox->isChecked());
    settings.setValue("reuseVis", ui->reuseVis_checkBox->isChecked());
    settings.setValue("theme", ui->theme_comboBox->currentIndex());
    settings.setValue("indentSize", ui->indentSize_spinBox->value());
    settings.setValue("wordWrap", ui->lineWrapping_checkBox->isChecked());
    settings.endGroup();

    settings.beginGroup("MainWindow");
    if (!_supportsDarkMode) {
        settings.setValue("darkMode", ui->darkMode_checkBox->isChecked());
    }
    settings.endGroup();

    settings.beginGroup("minizinc");
    settings.setValue("mznpath", ui->mznDistribPath->text());
    settings.endGroup();
}

void PreferencesDialog::updateSolverLabel()
{
    if (!_solversPopulated || _editingSolverIndex == -1) {
        return;
    }
    auto name = ui->name->text().isEmpty() ? "Untitled Solver" : ui->name->text();
    ui->solvers_combo->setItemText(_editingSolverIndex, name + " " + ui->version->text());
}


void PreferencesDialog::on_darkMode_checkBox_stateChanged(int checked)
{
    if (_supportsDarkMode) {
        return;
    }
    bool dark = checked == Qt::Checked;
    auto* mw = qobject_cast<MainWindow*>(parentWidget());
    if (mw != nullptr && mw->isDarkMode() != dark) {
        mw->setDarkMode(dark);
    }
    _ce->setDarkMode(dark);
}
