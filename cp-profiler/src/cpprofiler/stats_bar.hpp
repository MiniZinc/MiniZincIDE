#include <QWidget>
#include <QLabel>
#include <thread>

#include "tree/node_tree.hh"
#include "tree/node_widget.hh"

namespace cpprofiler
{

class NodeStatsBar : public QWidget
{

    const tree::NodeStats &stats;
    /// Status bar label for maximum depth indicator
    QLabel *depthLabel;
    /// Status bar label for number of solutions
    QLabel *solvedLabel;
    /// Status bar label for number of failures
    QLabel *failedLabel;
    /// Status bar label for number of skipped nodes
    QLabel *skippedLabel;
    /// Status bar label for number of choices
    QLabel *choicesLabel;
    /// Status bar label for number of open nodes
    QLabel *openLabel;

  public:
    NodeStatsBar(QWidget *parent, const tree::NodeStats &ns) : QWidget(parent), stats(ns)
    {

        using namespace tree;

        QHBoxLayout *hbl = new QHBoxLayout{this};
        hbl->setContentsMargins(2, 1, 2, 1);

        hbl->addWidget(new QLabel("Depth:"));
        depthLabel = new QLabel("0");
        hbl->addWidget(depthLabel);

        solvedLabel = new QLabel("0");
        hbl->addWidget(new NodeWidget(NodeStatus::SOLVED));
        hbl->addWidget(solvedLabel);

        failedLabel = new QLabel("0");
        hbl->addWidget(new NodeWidget(NodeStatus::FAILED));
        hbl->addWidget(failedLabel);

        skippedLabel = new QLabel("0");
        hbl->addWidget(new NodeWidget(NodeStatus::SKIPPED));
        hbl->addWidget(skippedLabel);

        choicesLabel = new QLabel("0");
        hbl->addWidget(new NodeWidget(NodeStatus::BRANCH));
        hbl->addWidget(choicesLabel);

        openLabel = new QLabel("0");
        hbl->addWidget(new NodeWidget(NodeStatus::UNDETERMINED));
        hbl->addWidget(openLabel);
    }

  public slots:

    void update()
    {
        depthLabel->setNum(stats.maxDepth());
        openLabel->setNum(stats.undeterminedCount());
        solvedLabel->setNum(stats.solvedCount());
        failedLabel->setNum(stats.failedCount());
        skippedLabel->setNum(stats.skippedCount());
        choicesLabel->setNum(stats.branchCount());
    }
};

} // namespace cpprofiler