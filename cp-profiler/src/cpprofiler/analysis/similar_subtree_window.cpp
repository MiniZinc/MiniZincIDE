#include "similar_subtree_window.hh"

#include <QGraphicsView>
#include <QVBoxLayout>
#include <QSplitter>
#include <QGraphicsSimpleTextItem>
#include <QDebug>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QAction>

#include <iostream>

#include "../tree/shape.hh"
#include "../tree/structure.hh"
#include "../tree/layout.hh"
#include "../tree/node_tree.hh"
#include "../utils/tree_utils.hh"

#include "path_comp.hh"

#include "subtree_pattern.hh"
#include "similar_subtree_analysis.hh"
#include "../tree/subtree_view.hh"

#include "histogram_scene.hh"
#include "../utils/perf_helper.hh"

using namespace cpprofiler::tree;

using std::vector;

namespace cpprofiler
{
namespace analysis
{

SimilarSubtreeWindow::SimilarSubtreeWindow(QWidget *parent, const tree::NodeTree &nt)
    : QDialog(parent), tree_(nt)
{

    histogram_.reset(new HistogramScene);
    m_subtree_view.reset(new SubtreeView{tree_});

    initInterface();

    detail::PerformanceHelper phelper;
    phelper.begin("analyse");
    analyse();
    phelper.end();

    phelper.begin("display patterns");
    displayPatterns();
    phelper.end();
}

static PatternProp str2Prop(const QString &str)
{
    if (str == "size")
        return PatternProp::SIZE;
    if (str == "count")
        return PatternProp::COUNT;
    if (str == "height")
        return PatternProp::HEIGHT;

    print("Error:: invalid property name, fall back to defaults.");

    return defaults::SORT_TYPE;
}

static QString prop2str(PatternProp prop)
{
    switch (prop)
    {
    case PatternProp::COUNT:
        return "count";
    case PatternProp::HEIGHT:
        return "height";
    case PatternProp::SIZE:
        return "size";
    }
}

void SimilarSubtreeWindow::initInterface()
{
    auto globalLayout = new QVBoxLayout{this};

    {
        auto settingsLayout = new QHBoxLayout{};
        globalLayout->addLayout(settingsLayout);

        settingsLayout->addWidget(new QLabel{"Similarity Criteria:"}, 0, Qt::AlignRight);

        auto typeChoice = new QComboBox();
        typeChoice->addItems({"contour", "subtree"});
        settingsLayout->addWidget(typeChoice);

        connect(typeChoice, &QComboBox::currentTextChanged, [this](const QString &str) {
            if (str == "contour")
            {
                m_sim_type = SimilarityType::SHAPE;
            }
            else if (str == "subtree")
            {
                m_sim_type = SimilarityType::SUBTREE;
            }
            analyse();
        });

        auto subsumedOption = new QCheckBox{"Keep subsumed"};
        settingsLayout->addWidget(subsumedOption);
        connect(subsumedOption, &QCheckBox::stateChanged, [this](int state) {
            settings_.subsumed = state;
            displayPatterns();
        });

        settingsLayout->addStretch();

        settingsLayout->addWidget(new QLabel{"Labels:"}, 0, Qt::AlignRight);

        auto labels_comp = new QComboBox{};
        labels_comp->addItems({"Ignore", "Vars only", "Full labels"});
        settingsLayout->addWidget(labels_comp);

        auto hideNotHighlighted = new QCheckBox{"Hide not selected"};
        hideNotHighlighted->setCheckState(Qt::Unchecked);
        settingsLayout->addWidget(hideNotHighlighted);

        connect(hideNotHighlighted, &QCheckBox::stateChanged, [this](int state) {
            settings_.hide_subtrees = state;
        });
    }

    auto splitter = new QSplitter{this};
    globalLayout->addWidget(splitter, 1);

    auto hist_view = new QGraphicsView{this};
    hist_view->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    hist_view->setScene(histogram_->scene());

    splitter->addWidget(hist_view);
    splitter->addWidget(m_subtree_view->widget());
    splitter->setSizes(QList<int>{1, 1});

    {
        globalLayout->addWidget(&path_line1_);
        globalLayout->addWidget(&path_line2_);
    }

    /// Filters interface
    {
        auto filtersLayout = new QHBoxLayout{};
        globalLayout->addLayout(filtersLayout);

        /// Height Filter
        auto heightFilter = new QSpinBox(this);
        heightFilter->setMinimum(defaults::MIN_SUBTREE_HEIGHT);
        heightFilter->setValue(defaults::MIN_SUBTREE_HEIGHT);
        filtersLayout->addWidget(new QLabel("min height"));
        filtersLayout->addWidget(heightFilter);

        connect(heightFilter, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int v) {
            filters_.min_height = v;
            displayPatterns();
        });

        /// Count Filter
        auto countFilter = new QSpinBox(this);
        countFilter->setMinimum(defaults::MIN_SUBTREE_COUNT);
        countFilter->setValue(defaults::MIN_SUBTREE_COUNT);
        filtersLayout->addWidget(new QLabel("min count"));
        filtersLayout->addWidget(countFilter);

        connect(countFilter, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int v) {
            filters_.min_count = v;
            displayPatterns();
        });

        filtersLayout->addStretch();

        /// Options for sorting
        auto sortChoiceCB = new QComboBox();
        sortChoiceCB->addItems({prop2str(PatternProp::SIZE),
                                prop2str(PatternProp::HEIGHT),
                                prop2str(PatternProp::COUNT)});
        sortChoiceCB->setCurrentText(prop2str(defaults::SORT_TYPE));
        filtersLayout->addWidget(new QLabel{"sort by: "});
        filtersLayout->addWidget(sortChoiceCB);

        connect(sortChoiceCB, &QComboBox::currentTextChanged, [this](const QString &str) {
            settings_.sort_type = str2Prop(str);
            displayPatterns();
        });

        /// Options for histogram
        auto histChoiceCB = new QComboBox();
        histChoiceCB->addItems({prop2str(PatternProp::SIZE),
                                prop2str(PatternProp::HEIGHT),
                                prop2str(PatternProp::COUNT)});
        histChoiceCB->setCurrentText(prop2str(defaults::HIST_TYPE));
        filtersLayout->addWidget(new QLabel{"histogram: "});
        filtersLayout->addWidget(histChoiceCB);

        connect(histChoiceCB, &QComboBox::currentTextChanged, [this](const QString &str) {
            settings_.hist_type = str2Prop(str);
            displayPatterns();
        });
    }

    auto keyDown = new QAction{"Press down", this};
    keyDown->setShortcuts({QKeySequence("Down"), QKeySequence("J")});
    addAction(keyDown);

    connect(keyDown, &QAction::triggered, histogram_.get(), &HistogramScene::nextPattern);

    auto keyUp = new QAction{"Press up", this};
    keyUp->setShortcuts({QKeySequence("Up"), QKeySequence("K")});
    addAction(keyUp);

    connect(keyUp, &QAction::triggered, histogram_.get(), &HistogramScene::prevPattern);

    connect(keyUp, &QAction::triggered, [this]() {
        update();
    });

    connect(histogram_.get(), &HistogramScene::should_be_highlighted,
            [this](const std::vector<NodeID> &nodes) {
                // if (settings_.hide_subtrees)
                // {
                emit should_be_highlighted(nodes, settings_.hide_subtrees);
                // }
            });

    connect(histogram_.get(), &HistogramScene::pattern_selected,
            m_subtree_view.get(), &SubtreeView::setNode);

    connect(histogram_.get(), &HistogramScene::should_be_highlighted,
            this, &SimilarSubtreeWindow::updatePathDiff);
}

SimilarSubtreeWindow::~SimilarSubtreeWindow() = default;

using std::vector;

std::ostream &operator<<(std::ostream &out, const Group &group)
{

    const auto size = group.size();
    if (size == 0)
    {
        return out;
    }
    for (auto i = 0; i < size - 1; ++i)
    {
        out << (int)group[i] << " ";
    }
    if (size > 0)
    {
        out << (int)group[size - 1];
    }
    return out;
}

std::ostream &operator<<(std::ostream &out, const vector<Group> &groups)
{

    const auto size = groups.size();

    if (size == 0)
        return out;
    for (auto i = 0; i < size - 1; ++i)
    {
        out << groups[i];
        out << "|";
    }

    out << groups[size - 1];

    return out;
}

std::ostream &operator<<(std::ostream &out, const Partition &partition)
{

    out << "[";
    out << partition.processed;
    out << "] [";
    out << partition.remaining;
    out << "]";

    return out;
}

static vector<SubtreePattern> eliminateSubsumed(const NodeTree &tree,
                                                const vector<SubtreePattern> &patterns)
{

    std::set<NodeID> marked;

    for (const auto &pattern : patterns)
    {
        for (auto nid : pattern.nodes())
        {

            const auto kids = tree.childrenCount(nid);
            for (auto alt = 0; alt < kids; ++alt)
            {
                auto kid = tree.getChild(nid, alt);
                marked.insert(kid);
            }
        }
    }

    vector<SubtreePattern> result;

    for (const auto &pattern : patterns)
    {

        const auto &nodes = pattern.nodes();

        /// Determine if all nodes are in 'marked':
        auto subset = true;

        for (auto nid : nodes)
        {
            if (marked.find(nid) == marked.end())
            {
                subset = false;
                break;
            }
        }

        if (!subset)
        {
            result.push_back(pattern);
        }
    }

    return result;
}

static std::unique_ptr<tree::Layout> computeShapes(const NodeTree &tree)
{
    auto layout = std::unique_ptr<tree::Layout>(new Layout);
    tree::VisualFlags vf;
    tree::LayoutComputer layout_c(tree, *layout, vf);
    layout_c.compute();
    return std::move(layout);
}

void SimilarSubtreeWindow::analyse()
{

    /// TODO: make sure building is finished

    result_.reset(new ss_analysis::Result);

    switch (m_sim_type)
    {
    case SimilarityType::SUBTREE:
    {
        *result_ = runIdenticalSubtrees(tree_);
    }
    break;
    case SimilarityType::SHAPE:
    {
        if (!layout_)
        {
            layout_ = computeShapes(tree_);
        }
        *result_ = runSimilarShapes(tree_, *layout_);
    }
    }

    print("patterns after analysis: {}", result_->size());

    /// Always remove trivial patterns
    detail::PerformanceHelper phelper;
    phelper.begin("remove trivial");
    auto new_end = std::remove_if(result_->begin(), result_->end(), [this](const SubtreePattern &pattern) {
        return pattern.count() < 2 ||
               pattern.height() < 2;
    });

    result_->resize(std::distance(result_->begin(), new_end));

    print("non-trivial patterns: {}", result_->size());
    phelper.end();
}

void SimilarSubtreeWindow::displayPatterns()
{
    /// should be initialized, but being defensive here
    if (!result_)
        return;

    detail::PerformanceHelper phelper;
    phelper.begin("make a copy");
    /// make a copy
    std::vector<SubtreePattern> patterns = *result_;
    phelper.end();

    /// apply filters
    auto new_end = std::remove_if(patterns.begin(), patterns.end(), [this](const SubtreePattern &pattern) {
        return pattern.count() < filters_.min_count ||
               pattern.height() < filters_.min_height;
    });
    patterns.resize(std::distance(patterns.begin(), new_end));

    /// See if subsumed patterns should be displayed
    if (!settings_.subsumed)
    {
        phelper.begin("subsumed elimination");
        /// NOTE: patterns should not contain patterns of cardinality 1
        /// before eliminating subsumed!
        patterns = eliminateSubsumed(tree_, patterns);
        phelper.end();
    }

    /// sort patterns
    switch (settings_.sort_type)
    {
    case PatternProp::COUNT:
        std::sort(patterns.begin(), patterns.end(),
                  [](const SubtreePattern &lhs, const SubtreePattern &rhs) {
                      return lhs.count() > rhs.count();
                  });
        break;
    case PatternProp::SIZE:
        std::sort(patterns.begin(), patterns.end(),
                  [](const SubtreePattern &lhs, const SubtreePattern &rhs) {
                      return lhs.size() > rhs.size();
                  });
        break;
    case PatternProp::HEIGHT:
        std::sort(patterns.begin(), patterns.end(),
                  [](const SubtreePattern &lhs, const SubtreePattern &rhs) {
                      return lhs.height() > rhs.height();
                  });
        break;
    }

    histogram_->reset();
    histogram_->setPatterns(std::move(patterns));
    histogram_->drawPatterns(settings_.hist_type);
}

/// Walk up the tree constructing label path
static std::vector<Label> labelPath(NodeID nid, const tree::NodeTree &tree)
{
    std::vector<Label> label_path;
    while (nid != NodeID::NoNode)
    {
        auto label = tree.getLabel(nid);
        if (label != emptyLabel)
        {
            label_path.push_back(label);
        }
        nid = tree.getParent(nid);
    }

    return label_path;
}

void SimilarSubtreeWindow::updatePathDiff(const std::vector<NodeID> &nodes)
{
    if (nodes.size() < 2)
        return;

    auto path_l = labelPath(nodes[0], tree_);
    auto path_r = labelPath(nodes[1], tree_);

    auto diff_pair = getLabelDiff(path_l, path_r);

    /// unique labels on path_l concatenated
    std::string text_l;

    for (auto &label : diff_pair.first)
    {
        text_l += label + " ";
    }

    std::string text_r;
    for (auto &label : diff_pair.second)
    {
        text_r += label + " ";
    }

    path_line1_.setText(text_l.c_str());
    path_line2_.setText(text_r.c_str());
}

} // namespace analysis
} // namespace cpprofiler