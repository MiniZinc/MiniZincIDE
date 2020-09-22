
#ifndef CPPROFILER_ANALYSIS_SIMILAR_SUBTREE_WINDOW_HH
#define CPPROFILER_ANALYSIS_SIMILAR_SUBTREE_WINDOW_HH

#include <QDialog>
#include <QLineEdit>
#include <memory>

#include "../core.hh"
#include "subtree_pattern.hh"

class QGraphicsScene;

namespace cpprofiler
{
namespace tree
{
class NodeTree;
class Layout;
class SubtreeView;
} // namespace tree
} // namespace cpprofiler

namespace cpprofiler
{
namespace analysis
{

struct SubtreePattern;

enum class SimilarityType
{
    SUBTREE,
    SHAPE
};

class HistogramScene;

/// Text line displaying difference on the path for two subtrees
class PathDiffLine : public QLineEdit
{
  public:
    PathDiffLine() : QLineEdit()
    {
        setReadOnly(true);
    }
};

namespace defaults
{
static constexpr int MIN_SUBTREE_COUNT = 2;
static constexpr int MIN_SUBTREE_HEIGHT = 2;
static constexpr PatternProp SORT_TYPE = PatternProp::SIZE;
static constexpr PatternProp HIST_TYPE = PatternProp::COUNT;
} // namespace defaults

namespace ss_analysis
{
using Result = std::vector<SubtreePattern>;
}

class SimilarSubtreeWindow : public QDialog
{
    Q_OBJECT
  private:
    // void analyse_shapes();

    const tree::NodeTree &tree_;
    std::unique_ptr<tree::Layout> layout_;

    std::unique_ptr<HistogramScene> histogram_;

    std::unique_ptr<tree::SubtreeView> m_subtree_view;

    /// Text line displaying difference on the path for two subtrees of a pattern
    PathDiffLine path_line1_;
    PathDiffLine path_line2_;

    // SimilarityType m_sim_type = SimilarityType::SUBTREE;
    SimilarityType m_sim_type = SimilarityType::SHAPE;

    struct SubtreeAnalysisFilters
    {
        int min_height = defaults::MIN_SUBTREE_HEIGHT;
        int min_count = defaults::MIN_SUBTREE_COUNT;
    } filters_;

    struct Settings
    {
        /// whether to show subsumed subtrees
        bool subsumed = false;
        /// whether not highlighted subtrees should be hidden
        bool hide_subtrees = false;
        /// currently selected option for sorting
        PatternProp sort_type = defaults::SORT_TYPE;
        /// currently selected option for histogram drawing (rectangle length)
        PatternProp hist_type = defaults::HIST_TYPE;
    } settings_;

    std::unique_ptr<ss_analysis::Result> result_;

    void initInterface();

    /// Apply filters, eliminate subsumed and display the result
    void displayPatterns();

  private slots:
    /// Calculate the difference in label paths for the first two nodes
    void updatePathDiff(const std::vector<NodeID> &nodes);

  public:
    SimilarSubtreeWindow(QWidget *parent, const tree::NodeTree &nt);

    ~SimilarSubtreeWindow();

    void analyse();

  signals:
    void should_be_highlighted(const std::vector<NodeID> &nodes, bool hide_rest);
};

} // namespace analysis
} // namespace cpprofiler

#endif
