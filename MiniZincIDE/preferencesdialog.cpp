#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

#include "exception.h"
#include "ideutils.h"
#include "mainwindow.h"
#include "ide.h"

#include <QMessageBox>
#include <QSettings>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>

PreferencesDialog::PreferencesDialog(bool addNewSolver, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);

    QSettings settings;

    // Get font so we can populate preview
    settings.beginGroup("MainWindow");
    QFont editorFont = IDEUtils::fontFromString(settings.value("editorFont").toString());
    auto zoom = settings.value("zoom", 100).toInt();
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
        auto& origTheme = IDE::instance()->themeManager->get(_origThemeIndex);
        _ce = new CodeEditor(nullptr, ":/cheat_sheet.mzn", false, false, editorFont, 2, false, origTheme, _origDarkMode, nullptr, this);
        _ce->document()->setPlainText(fileContents);
        _ce->setReadOnly(true);
        ui->preview_verticalLayout->addWidget(_ce);
    }

    // Now everything else can get populated

    // Separate font family from size so that combo box stays the same size
    auto font = qApp->font();
    font.setFamily(editorFont.family());
    ui->fontComboBox->setCurrentFont(font);
    ui->fontSize_spinBox->setValue(editorFont.pointSize());
    ui->zoom_spinBox->setValue(zoom);

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
    ui->visPort_spinBox->setValue(settings.value("visPort", 3000).toInt());
    ui->visWsPort_spinBox->setValue(settings.value("visWsPort", 3100).toInt());
    ui->visUrl_checkBox->setChecked(settings.value("printVisUrl", false).toBool());
    ui->printCommand_checkBox->setChecked(settings.value("printCommand", false).toBool());
    ui->indentSize_spinBox->setValue(settings.value("indentSize", 2).toInt());
    bool indentTabs = settings.value("indentTabs", false).toBool();
    ui->indentSpaces_radioButton->setChecked(!indentTabs);
    ui->indentTabs_radioButton->setChecked(indentTabs);
    ui->lineWrapping_checkBox->setChecked(settings.value("wordWrap", true).toBool());
    _origThemeIndex = settings.value("theme", 0).toInt();
    ui->theme_comboBox->setCurrentIndex(_origThemeIndex);

    auto* d = IDE::instance()->darkModeNotifier;
    if (d->hasSystemSetting()) {
        _origDarkMode = d->darkMode();
        ui->darkMode_checkBox->hide();
    }
    auto* t = IDE::instance()->themeManager;
    _ce->setTheme(t->get(_origThemeIndex), _origDarkMode);
    ui->darkMode_checkBox->setChecked(_origDarkMode);
    settings.endGroup();
    // Load user solver search paths
    ui->configuration_groupBox->setEnabled(driver.isValid());
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
        if (obj.contains("solverDefaults")) {
            for (auto it : obj["solverDefaults"].toArray()) {
                auto arr = it.toArray();
                auto solverId = arr[0].toString();
                auto flag = arr[1].toString();
                _userDefaultFlags.insert(solverId, flag);
            }
        }
    }

    IDEUtils::watchChildChanges(ui->solverFrame, this, [=] () {
        auto& driver = MznDriver::get();
        auto& solvers = driver.solvers();
        if (!solvers.isEmpty()) {
            _editingSolverIndex = ui->solvers_combo->currentIndex();
        }
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
    updateCodeEditorFont();
}


void PreferencesDialog::on_fontSize_spinBox_valueChanged(int size)
{
    updateCodeEditorFont();
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

void PreferencesDialog::updateCodeEditorFont()
{
    auto editorFont = ui->fontComboBox->currentFont();
    editorFont.setPointSize(ui->fontSize_spinBox->value() * ui->zoom_spinBox->value() / 100);
    _ce->setEditorFont(editorFont);
}

void PreferencesDialog::on_theme_comboBox_currentIndexChanged(int index)
{
    auto* t = IDE::instance()->themeManager;
    t->current(index);
    IDE::instance()->refreshTheme();
    _ce->setTheme(t->current(), ui->darkMode_checkBox->isChecked());
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

void PreferencesDialog::loadDriver(bool showError)
{
    auto& driver = MznDriver::get();
    auto& solvers = driver.solvers();

    _solversPopulated = false;
    auto dest = ui->solvers_combo->currentIndex();
    bool destExists = dest < solvers.size();
    bool addNew = dest == ui->solvers_combo->count() - 1;
    QString matchId;
    QString matchVersion;
    if (dest == _editingSolverIndex) {
        // Was editing, so use widget values
        matchId = ui->solverId->text();
        matchVersion = ui->version->text();
        destExists = true;
    } else if (destExists) {
        // Switching to different solver
        matchId = solvers[dest]->id;
        matchVersion = solvers[dest]->version;
    }
    try {
        driver.setLocation(ui->mznDistribPath->text());
        ui->mzn2fzn_version->setText(driver.minizincVersionString());
    } catch (Exception& e) {
        if (showError) {
            showMessageBox(e.message());
        }
        ui->mzn2fzn_version->setText(e.message());
    }

    // Load user solver search paths
    ui->extraSearchPath_listWidget->clear();
    ui->configuration_groupBox->setEnabled(driver.isValid());
    _userDefaultFlags.clear();
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
        if (obj.contains("solverDefaults")) {
            for (auto it : obj["solverDefaults"].toArray()) {
                auto arr = it.toArray();
                auto solverId = arr[0].toString();
                auto flag = arr[1].toString();
                _userDefaultFlags.insert(solverId, flag);
            }
        }
    }

    populateSolvers();
    int index = addNew ? ui->solvers_combo->count() - 1 : 0;
    if (destExists) {
        int i = 0;
        for (auto* solver : solvers) {
            if (solver->id == matchId && solver->version == matchVersion) {
                index = i;
                break;
            }
            i++;
        }
    }

    if (ui->solvers_combo->currentIndex() == index) {
        on_solvers_combo_currentIndexChanged(index);
    } else {
        ui->solvers_combo->setCurrentIndex(index);
    }
}

void PreferencesDialog::populateSolvers()
{
    if (_solversPopulated) {
        return;
    }

    _editingSolverIndex = -1;

    auto& driver = MznDriver::get();
    auto& solvers = driver.solvers();

    ui->solvers_combo->clear();

    for (auto& solver : solvers) {
        ui->solvers_combo->addItem(solver->name + " " + solver->version);
    }

    if (solvers.empty()) {
        ui->solvers_combo->addItem("(No solvers found)");
    }

    ui->solvers_combo->addItem("Add new...");

    ui->solvers_tab->setEnabled(driver.isValid());

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
        // Update the required flags/user set flags

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

    }

    if (ui->solverFrame->isEnabled()) {
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
            if (i != index && ui->solverId->text().trimmed()==solvers[i]->id) {
                showMessageBox("A solver with that solver ID already exists.");
                return false;
            }
        }
        if (index==solvers.size()) {
            auto* s = new Solver;
            s->configFile = userSolverConfigDir+"/"+ui->solverId->text().trimmed()+".msc";
            if (!QDir().mkpath(userSolverConfigDir)) {
                showMessageBox("Cannot create user configuration directory "+userSolverConfigDir);
                return false;
            }
            solvers.append(s);
            allowFileRestore(s->configFile);
        }

        solvers[index]->executable = ui->executable->text().trimmed();
        solvers[index]->mznlib = ui->mznpath->text();
        solvers[index]->name = ui->name->text().trimmed();
        solvers[index]->id = ui->solverId->text().trimmed();
        solvers[index]->version = ui->version->text().trimmed();
        solvers[index]->isGUIApplication= ui->detach->isChecked();
        Solver::SolverInputType inputTypeMap[4] = { Solver::I_FZN, Solver::I_JSON, Solver::I_MZN, Solver::I_NL };
        solvers[index]->inputType = inputTypeMap[ui->inputType_comboBox->currentIndex()];
        solvers[index]->needsSolns2Out = ui->needs_solns2out->isChecked();

        solvers[index]->stdFlags.removeAll("-a");
        if (ui->has_stdflag_a->isChecked()) {
            solvers[index]->stdFlags.push_back("-a");
        }
        solvers[index]->stdFlags.removeAll("-n");
        if (ui->has_stdflag_n->isChecked()) {
            solvers[index]->stdFlags.push_back("-n");
        }
        solvers[index]->stdFlags.removeAll("-p");
        if (ui->has_stdflag_p->isChecked()) {
            solvers[index]->stdFlags.push_back("-p");
        }
        solvers[index]->stdFlags.removeAll("-s");
        if (ui->has_stdflag_s->isChecked()) {
            solvers[index]->stdFlags.push_back("-s");
        }
        solvers[index]->stdFlags.removeAll("-v");
        if (ui->has_stdflag_v->isChecked()) {
            solvers[index]->stdFlags.push_back("-v");
        }
        solvers[index]->stdFlags.removeAll("-r");
        if (ui->has_stdflag_r->isChecked()) {
            solvers[index]->stdFlags.push_back("-r");
        }
        solvers[index]->stdFlags.removeAll("-f");
        if (ui->has_stdflag_f->isChecked()) {
            solvers[index]->stdFlags.push_back("-f");
        }
        solvers[index]->stdFlags.removeAll("-t");
        if (ui->has_stdflag_t->isChecked()) {
            solvers[index]->stdFlags.push_back("-t");
        }

        QJsonObject json = solvers[index]->json;
        json.remove("extraInfo");
        json["executable"] = ui->executable->text().trimmed();
        json["mznlib"] = ui->mznpath->text();
        json["name"] = ui->name->text().trimmed();
        json["id"] = ui->solverId->text().trimmed();
        json["version"] = ui->version->text().trimmed();
        json["isGUIApplication"] = ui->detach->isChecked();
        QStringList inputTypeStringMap = { "FZN", "JSON", "MZN", "NL" };
        json["inputType"] = inputTypeStringMap[ui->inputType_comboBox->currentIndex()];
        json.remove("supportsFzn");
        json.remove("supportsMzn");
        json.remove("supportsNL");
        json["needsSolns2Out"] = ui->needs_solns2out->isChecked();
        json["stdFlags"] = QJsonArray::fromStringList(solvers[index]->stdFlags);
        QJsonDocument jdoc(json);
        QFile jdocFile(solvers[index]->configFile);
        allowFileRestore(solvers[index]->configFile);
        if (!jdocFile.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
            showMessageBox("Cannot save configuration file "+solvers[index]->configFile);
            return false;
        }
        if (jdocFile.write(jdoc.toJson())==-1) {
            showMessageBox("Cannot save configuration file "+solvers[index]->configFile);
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
        if (solvers.size() == 0) {
            // No solver to switch back to, so create dummy
            ui->solvers_combo->insertItem(0, "(No solvers found)");
            index++;
        }
        ui->solvers_combo->setCurrentIndex(index > 0 ? index - 1 : 0);
        ui->solvers_combo->removeItem(index);
        return;
    }
    if (QMessageBox::warning(this,
                                                       "MiniZinc IDE",
                                                       "Delete solver " + solvers[index]->name + "?",
                                                       QMessageBox::Ok | QMessageBox::Cancel)
            == QMessageBox::Ok) {
        auto configFile = solvers[index]->configFile;
        allowFileRestore(configFile);
        QFile sf(configFile);
        if (!sf.remove()) {
            showMessageBox("Cannot remove configuration file "+solvers[index]->configFile);
            return;
        }
        solvers.removeAt(index);
        if (solvers.size() == 0) {
            // No solver to switch back to, so create dummy
            ui->solvers_combo->insertItem(0, "(No solvers found)");
            index++;
        }
        ui->solvers_combo->setCurrentIndex(index > 0 ? index - 1 : 0);
        ui->solvers_combo->removeItem(index);
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

    QFileInfo uc_info(userConfigFile);
    if (!QDir().mkpath(uc_info.absoluteDir().absolutePath())) {
        showMessageBox("Cannot create user configuration directory "+uc_info.absoluteDir().absolutePath());
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
        ui->name->setText(solvers[index]->name);
        ui->solverId->setText(solvers[index]->id);
        ui->version->setText(solvers[index]->version);
        ui->executable->setText(solvers[index]->executable);
        ui->exeNotFoundLabel->setVisible(!solvers[index]->executable.isEmpty() && solvers[index]->executable_resolved.isEmpty());
        ui->detach->setChecked(solvers[index]->isGUIApplication);
        switch (solvers[index]->inputType) {
        case Solver::I_FZN:
            ui->inputType_comboBox->setCurrentIndex(0);
            break;
        case Solver::I_JSON:
            ui->inputType_comboBox->setCurrentIndex(1);
            break;
        case Solver::I_MZN:
            ui->inputType_comboBox->setCurrentIndex(2);
            break;
        case Solver::I_NL:
            ui->inputType_comboBox->setCurrentIndex(3);
            break;
        default:
            ui->inputType_comboBox->setCurrentIndex(0);
            break;
        }
        ui->needs_solns2out->setChecked(solvers[index]->needsSolns2Out);
        ui->mznpath->setText(solvers[index]->mznlib);
        bool solverConfigIsUserEditable = false;
        if (!solvers[index]->configFile.isEmpty()) {
            QFileInfo configFileInfo(solvers[index]->configFile);
            if (configFileInfo.canonicalPath().startsWith(userSolverConfigCanonical)) {
                solverConfigIsUserEditable = true;
            }
        }

        ui->deleteButton->setEnabled(solverConfigIsUserEditable);
        ui->solverFrame->setEnabled(solverConfigIsUserEditable);

        ui->has_stdflag_a->setChecked(solvers[index]->stdFlags.contains("-a"));
        ui->has_stdflag_p->setChecked(solvers[index]->stdFlags.contains("-p"));
        ui->has_stdflag_r->setChecked(solvers[index]->stdFlags.contains("-r"));
        ui->has_stdflag_n->setChecked(solvers[index]->stdFlags.contains("-n"));
        ui->has_stdflag_s->setChecked(solvers[index]->stdFlags.contains("-s"));
        ui->has_stdflag_f->setChecked(solvers[index]->stdFlags.contains("-f"));
        ui->has_stdflag_v->setChecked(solvers[index]->stdFlags.contains("-v"));
        ui->has_stdflag_t->setChecked(solvers[index]->stdFlags.contains("-t"));

        auto flags = _userDefaultFlags.values(solvers[index]->id);
        for (auto& rf : solvers[index]->requiredFlags) {
            if (!flags.contains(rf)) {
                flags << rf;
            }
        }
        if (flags.empty()) {
            ui->requiredFlags->hide();
        } else {
            ui->requiredFlags->show();
            int row = 0;
            for (auto& rf : flags) {
                QString val;
                int foundFlag = solvers[index]->defaultFlags.indexOf(rf);
                if (foundFlag != -1 && foundFlag < solvers[index]->defaultFlags.size()-1) {
                    val = solvers[index]->defaultFlags[foundFlag+1];
                }
                rfLayout->addWidget(new QLabel(rf), row, 0);
                rfLayout->addWidget(new QLineEdit(val), row, 1);
                row++;
            }

            IDEUtils::watchChildChanges(ui->requiredFlags, this, [=] () {
                auto& driver = MznDriver::get();
                auto& solvers = driver.solvers();
                if (!solvers.isEmpty()) {
                    _editingSolverIndex = ui->solvers_combo->currentIndex();
                }
            });
        }
        _editingSolverIndex = -1;
    } else {
        ui->name->setText("");
        ui->solverId->setText("");
        ui->version->setText("");
        ui->executable->setText("");
        ui->detach->setChecked(false);
        ui->inputType_comboBox->setCurrentIndex(0);
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

        bool addNew = index == ui->solvers_combo->count() - 1;

        if (addNew) {
            if (solvers.isEmpty()) {
                // Remove dummy solver
                ui->solvers_combo->removeItem(0);
                return;
            }
            ui->solvers_combo->setItemText(index, "New solver");
            ui->solvers_combo->addItem("Add new...");
            _editingSolverIndex = index;
        } else {
            _editingSolverIndex = -1;
        }

        ui->deleteButton->setEnabled(addNew);
        ui->solverFrame->setEnabled(addNew);
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
    // Undo theme changes
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
    settings.setValue("visPort", ui->visPort_spinBox->value());
    settings.setValue("visWsPort", ui->visWsPort_spinBox->value());
    settings.setValue("printVisUrl", ui->visUrl_checkBox->isChecked());
    settings.setValue("theme", ui->theme_comboBox->currentIndex());
    settings.setValue("indentTabs", ui->indentTabs_radioButton->isChecked());
    settings.setValue("indentSize", ui->indentSize_spinBox->value());
    settings.setValue("wordWrap", ui->lineWrapping_checkBox->isChecked());
    settings.endGroup();

    settings.beginGroup("MainWindow");
    settings.setValue("darkMode", ui->darkMode_checkBox->isChecked());
    auto editorFont = ui->fontComboBox->currentFont();
    editorFont.setPointSize(ui->fontSize_spinBox->value());
    settings.setValue("editorFont", editorFont.toString());
    settings.setValue("zoom", ui->zoom_spinBox->value());
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
    auto* d = IDE::instance()->darkModeNotifier;
    auto* t = IDE::instance()->themeManager;
    bool dark = checked == Qt::Checked;
    d->requestChangeDarkMode(dark);
    _ce->setTheme(t->current(), d->darkMode());
}

void PreferencesDialog::on_zoom_spinBox_valueChanged(int value)
{
    updateCodeEditorFont();
}
