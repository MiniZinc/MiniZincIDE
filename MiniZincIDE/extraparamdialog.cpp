#include "extraparamdialog.h"
#include "ui_extraparamdialog.h"

#include <QMouseEvent>

ExtraParamDialog::ExtraParamDialog(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::ExtraParamDialog)
{
    ui->setupUi(this);
    _sourceModel = new QStandardItemModel(this);
    _filterModel = new QSortFilterProxyModel(this);
    _filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    _filterModel->setSourceModel(_sourceModel);
    _filterModel->setFilterKeyColumn(1);
    ui->params_listView->setModel(_filterModel);

    connect(ui->params_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ExtraParamDialog::on_selectionChanged);
}

ExtraParamDialog::~ExtraParamDialog()
{
    delete ui;
}

void ExtraParamDialog::setParams(const QList<SolverFlag>& params) {
    _sourceModel->clear();
    _sourceModel->setRowCount(params.length());
    _sourceModel->setColumnCount(2);

    for (int i = 0; i < params.length(); i++) {
        auto* item = new QStandardItem(params[i].description);
        item->setFlags(Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled);
        item->setData(QVariant::fromValue(params[i]));
        item->setToolTip(params[i].name);
        _sourceModel->setItem(i, 0, item);

        // Allow filtering to search by flag name and description
        auto* searchItem = new QStandardItem(params[i].name + " " + params[i].description);
        _sourceModel->setItem(i, 1, searchItem);
    }
}

void ExtraParamDialog::setParamEnabled(const SolverFlag& param, bool enabled) {
    for (int i = 0; i < _sourceModel->rowCount(); i++) {
        auto* item = _sourceModel->item(i);
        auto flag = qvariant_cast<SolverFlag>(item->data());
        if (flag.name == param.name) {
            item->setFlags(enabled ? Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEnabled : Qt::ItemFlag::NoItemFlags);
            return;
        }
    }
}

void ExtraParamDialog::on_selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
    // The selected argument is wrong for some reason, so just check the selected indexes manually
    bool hasSelection = ui->params_listView->selectionModel()->selectedIndexes().empty();
    ui->add_pushButton->setDisabled(hasSelection);
}

void ExtraParamDialog::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    ui->filter_lineEdit->setFocus();
}

void ExtraParamDialog::on_filter_lineEdit_textEdited(const QString& pattern)
{
    _filterModel->setFilterFixedString(pattern);
}

void ExtraParamDialog::on_addAll_pushButton_clicked()
{
    QList<SolverFlag> flags;
    for (int i = 0; i < _sourceModel->rowCount(); i++) {
        auto* item = _sourceModel->item(i);
        if (item->isEnabled()) {
            auto sf = qvariant_cast<SolverFlag>(item->data());
            flags << sf;
        }
    }
    emit addParams(flags);
    resetControls();
}

void ExtraParamDialog::on_customParameter_pushButton_clicked()
{
    emit addCustomParam();
    resetControls();
}

void ExtraParamDialog::on_add_pushButton_clicked()
{
    QList<SolverFlag> flags;
    for (auto item : ui->params_listView->selectionModel()->selectedIndexes()) {
        if (item.column() == 0) {
            auto sf = qvariant_cast<SolverFlag>(item.data(Qt::UserRole + 1));
            flags << sf;
        }
    }
    emit addParams(flags);
    resetControls();
}

void ExtraParamDialog::resetControls()
{
    ui->filter_lineEdit->clear();
    ui->params_listView->selectionModel()->clear();
    _filterModel->setFilterFixedString("");
}

void ExtraParamDialog::mousePressEvent(QMouseEvent* event)
{
    // Hack which stops the menu from closing when clicking on the widget empty space
    event->accept();
}

void ExtraParamDialog::mouseReleaseEvent(QMouseEvent* event)
{
    // Hack which stops the menu from closing when clicking on the widget empty space
    event->accept();
}
