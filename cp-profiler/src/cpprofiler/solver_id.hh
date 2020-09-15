namespace cpprofiler
{

/// Mimics UID
struct SolverID
{
    // Node number
    int32_t nid;
    // Restart id
    int32_t rid;
    // Thread id
    int32_t tid;

    std::string toString() const
    {
        return std::string("{") + std::to_string(nid) +
               ", " + std::to_string(rid) + ", " + std::to_string(tid) + "}";
    }
};

static bool operator==(const SolverID &lhs, const SolverID &rhs)
{
    return (lhs.nid == rhs.nid) && (lhs.tid == rhs.tid) && (lhs.rid == rhs.rid);
}

} // namespace cpprofiler

namespace std
{
template <>
struct hash<cpprofiler::SolverID>
{
    size_t operator()(cpprofiler::SolverID const &a) const
    {
        const size_t h1(std::hash<int>{}(a.nid));
        size_t const h2(std::hash<int>{}(a.tid));
        size_t const h3(std::hash<int>{}(a.rid));
        return h1 ^ (h2 << 1) ^ (h3 << 1);
    }
};
} // namespace std