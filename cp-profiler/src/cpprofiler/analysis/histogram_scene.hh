#ifndef CPPROFILER_ANALYSES_HISTOGRAM_SCENE
#define CPPROFILER_ANALYSES_HISTOGRAM_SCENE

#include <memory>
#include <unordered_map>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>

#include "../core.hh"
#include "subtree_pattern.hh"

namespace cpprofiler
{
namespace analysis
{

/// vertical distance between two shape rectangles
static constexpr int V_DISTANCE = 2;

class PatternRect;
enum class PatternProp;

using PatternRectPtr = std::shared_ptr<PatternRect>;
using PatternPtr = std::shared_ptr<SubtreePattern>;

class HistogramScene : public QObject
{
  Q_OBJECT
  /// Scene used for drawing
  std::unique_ptr<QGraphicsScene> scene_;

  /// Selected pattern rectangle (nullptr if no selected)
  PatternRect *selected_rect_ = nullptr;
  /// Index of the selected pattern
  int selected_idx_ = -1;

  /// A list of visual elements for patterns (used for navigation);
  std::vector<PatternRectPtr> rects_;

  /// The result of the analysis in a form of a list of patterns
  std::vector<PatternPtr> patterns_;

  /// Mapping from a visual element to the pattern it represents
  std::unordered_map<PatternRectPtr, PatternPtr> rect2pattern_;

  /// Find the possition of the rectangle representing `pattern`
  int findPatternIdx(PatternRect *pattern) const;

  /// Select `idx` unselecting the previous pattern
  void changeSelectedPattern(int idx);

  PatternPtr rectToPattern(PatternRectPtr prect) const;

public:
  HistogramScene()
  {
    scene_.reset(new QGraphicsScene());
  }

  /// Prepare the GUI for new patterns
  void reset();

  /// Expose underlying scene
  QGraphicsScene *scene()
  {
    return scene_.get();
  }

  /// Find the pattern's id and select it
  void findAndSelect(PatternRect *prect);

  /// Draw the patterns onto the scene
  void drawPatterns(PatternProp prop);

  /// Initialize patterns based on the analysis results
  void setPatterns(std::vector<SubtreePattern> &&patterns);

signals:

  void pattern_selected(NodeID);
  void should_be_highlighted(const std::vector<NodeID> &);

public slots:

  void prevPattern();
  void nextPattern();
};

} // namespace analysis
} // namespace cpprofiler

#endif