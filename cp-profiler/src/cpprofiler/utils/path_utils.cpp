#include "path_utils.hh"
#include "string_utils.hh"

namespace cpprofiler
{

namespace utils {

    using std::vector;
    using std::string;

    PathPair getPathPair(const string& path, bool omitDecomp) {

        PathPair ph;
        if(path.empty()) return ph;

        vector<string> path_split = utils::split(path, major_sep);

        string mzn_file;
        size_t idx = 0;
        for (; idx < path_split.size(); ++idx) {
            const auto& path_head = path_split[idx];

            const auto head = utils::split(path_head, minor_sep);

            string head_file = head[0];
            if (idx == 0) mzn_file = head[0];

            if (head_file != mzn_file) break;

            ph.model_level.push_back(path_head);
        }

        if (!omitDecomp) {
            for (; idx < path_split.size(); ++idx) {
                ph.decomp_level.push_back(path_split[idx]);
            }
        }

        return ph;
    }

}
}