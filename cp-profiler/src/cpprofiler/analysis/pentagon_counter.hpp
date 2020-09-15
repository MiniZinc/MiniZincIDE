#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include "../tree/node_widget.hh"

namespace cpprofiler
{
namespace analysis
{

class PentagonCounter : public QWidget
{

    QLabel *pentagonCount;

  public:
    PentagonCounter(QWidget *parent) : QWidget(parent)
    {

        using namespace tree;

        QHBoxLayout *hbl = new QHBoxLayout{this};
        hbl->setContentsMargins(2, 1, 2, 1);

        hbl->addWidget(new NodeWidget(NodeStatus::MERGED));
        pentagonCount = new QLabel("0");
        hbl->addWidget(pentagonCount);
    }

    void update(int count)
    {
        pentagonCount->setNum(count);
    }
};

} // namespace analysis
} // namespace cpprofiler