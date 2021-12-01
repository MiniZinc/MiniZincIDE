#ifndef EXTRAPARAMDIALOG_H
#define EXTRAPARAMDIALOG_H

#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include "solver.h"

namespace Ui {
class ExtraParamDialog;
}

class ExtraParamDialog : public QWidget
{
    Q_OBJECT

public:
    explicit ExtraParamDialog(QWidget* parent = nullptr);
    ~ExtraParamDialog() override;

    void showEvent(QShowEvent* event) override;

signals:
    void addParams(const QList<SolverFlag>& params);
    void addCustomParam();

public slots:
    void setParams(const QList<SolverFlag>& params);
    void setParamEnabled(const SolverFlag& param, bool enabled);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void on_filter_lineEdit_textEdited(const QString& pattern);
    void on_selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

    void on_addAll_pushButton_clicked();

    void on_customParameter_pushButton_clicked();

    void on_add_pushButton_clicked();

    void resetControls();

private:
    Ui::ExtraParamDialog* ui;
    QStandardItemModel* _sourceModel;
    QSortFilterProxyModel* _filterModel;
};

#endif // EXTRAPARAMDIALOG_H
