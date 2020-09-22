#pragma once

#include <QObject>

namespace cpprofiler
{

class Message;
class Execution;

class TreeBuilder : public QObject
{
    Q_OBJECT

    Execution &m_execution;

    /// Need to keep track of the number of restarts;
    /// cannot rely on solvers to send correct restart id
    /// (e.g. Chuffed doesn't do that)
    int restart_count = 0;

  public:
    TreeBuilder(Execution &ex);

    void startBuilding();

    void finishBuilding();

    void handleNode(Message *node);

  signals:

    void buildingDone();
};

} // namespace cpprofiler