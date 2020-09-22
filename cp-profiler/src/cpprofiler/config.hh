#pragma once

#include <algorithm>

namespace cpprofiler
{
namespace tree
{

namespace layout
{
constexpr int dist_y = 36;
constexpr int min_dist_x = 16;
} // namespace layout

namespace traditional
{
constexpr int MAX_NODE_W = 22;
constexpr int HALF_MAX_NODE_W = MAX_NODE_W / 2;

constexpr int BRANCH_WIDTH = 18;
constexpr int HALF_BRANCH_W = BRANCH_WIDTH / 2;

constexpr int FAILED_WIDTH = 14;
constexpr int HALF_FAILED_WIDTH = FAILED_WIDTH / 2;

constexpr int SOL_WIDTH = 18;
constexpr int HALF_SOL_W = SOL_WIDTH / 2;

constexpr int UNDET_WIDTH = BRANCH_WIDTH;
constexpr int HALF_UNDET_WIDTH = BRANCH_WIDTH / 2;

constexpr int SKIPPED_WIDTH = FAILED_WIDTH;
constexpr int HALF_SKIPPED_WIDTH = SKIPPED_WIDTH / 2;

constexpr int COLLAPSED_WIDTH = 36;
constexpr int HALF_COLLAPSED_WIDTH = COLLAPSED_WIDTH / 2;
constexpr int COLLAPSED_DEPTH = layout::dist_y + 14;

constexpr int PENTAGON_WIDTH = BRANCH_WIDTH;
constexpr int PENTAGON_HALF_W = PENTAGON_WIDTH / 2;
constexpr int PENTAGON_THIRD_W = PENTAGON_WIDTH / 3;

constexpr int BIG_PENTAGON_WIDTH = MAX_NODE_W;
constexpr int BIG_PENTAGON_HALF_W = BIG_PENTAGON_WIDTH / 2;
constexpr int BIG_PENTAGON_THIRD_W = BIG_PENTAGON_WIDTH / 3;

/// there is no constexpr std::max until C++14
static_assert(MAX_NODE_W >= BRANCH_WIDTH,   "MAX_NODE_W >= BRANCH_WIDTH");
static_assert(MAX_NODE_W >= FAILED_WIDTH,   "MAX_NODE_W >= FAILED_WIDTH");
static_assert(MAX_NODE_W >= SOL_WIDTH,      "MAX_NODE_W >= SOL_WIDTH");
static_assert(MAX_NODE_W >= UNDET_WIDTH,    "MAX_NODE_W >= UNDET_WIDTH");
static_assert(MAX_NODE_W >= PENTAGON_WIDTH, "MAX_NODE_W >= PENTAGON_WIDTH");

constexpr int SHADOW_OFFSET = 3.0;
} // namespace traditional

namespace lantern
{
static constexpr int BASE_HEIGHT = 14;
static constexpr int HALF_WIDTH = 20;
static constexpr int PRECISION = 127;
static constexpr int MAX_LEVELS = 5;
/// delta height for lanterns with respect to the size increase by 1
static constexpr float K = (float)(layout::dist_y * (MAX_LEVELS - 1) - BASE_HEIGHT) / PRECISION;
} // namespace lantern

} // namespace tree
} // namespace cpprofiler