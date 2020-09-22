#include "path_comp.hh"
#include <algorithm>

using std::vector;

namespace cpprofiler
{
namespace analysis
{

/// A wrapper around std::set_intersection; copying is intended
static vector<Label> set_intersect(vector<Label> v1, vector<Label> v2)
{
    /// set_intersection requires the resulting set to be
    /// at least as large as the smallest of the two sets
    vector<Label> res(std::min(v1.size(), v2.size()));

    std::sort(begin(v1), end(v1));
    std::sort(begin(v2), end(v2));

    auto it = std::set_intersection(begin(v1), end(v1), begin(v2), end(v2),
                                    begin(res));
    res.resize(it - begin(res));

    return res;
}

/// A wrapper around std::set_symemtric_diff; copying is intended
static vector<Label> set_symmetric_diff(vector<Label> v1, vector<Label> v2)
{
    vector<Label> res(v1.size() + v2.size());

    /// vectors must be sorted
    std::sort(begin(v1), end(v1));
    std::sort(begin(v2), end(v2));

    /// fill `res` with elements not present in both v1 and v2
    auto it = std::set_symmetric_difference(begin(v1), end(v1), begin(v2), end(v2),
                                            begin(res));

    res.resize(it - begin(res));

    return res;
}

/// Calculate unique labels for both paths (present in one but not in the other)
std::pair<vector<Label>, vector<Label>>
getLabelDiff(const std::vector<Label> &path1,
             const std::vector<Label> &path2)
{
    auto diff = set_symmetric_diff(path1, path2);

    auto unique_1 = set_intersect(path1, diff);

    auto unique_2 = set_intersect(path2, diff);

    return std::make_pair(std::move(unique_1), std::move(unique_2));
}

} // namespace analysis
} // namespace cpprofiler