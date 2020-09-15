#include "../tree/node_tree.hh"
#include "../tree/structure.hh"

#include "../utils/array.hh"
#include "../utils/debug.hh"

#include <cassert>

namespace cpprofiler
{
namespace tests
{
namespace tree_test
{

class TestClass
{
  private:
    int m_id;
    static int counter;

  public:
    TestClass()
    {
        counter++;
        m_id = counter;
        print("TestClass(): {}", m_id);
    }

    ~TestClass()
    {
        print("~TestClass(): {}", m_id);
    }

    TestClass(const TestClass &other)
    {
        counter++;
        m_id = counter;
        print("TestClass(const T&): {}", m_id);
    }

    TestClass(TestClass &&other)
    {
        m_id = other.m_id;
        print("TestClass(const T&&): {}", m_id);
    }

    TestClass &operator=(const TestClass &other)
    {
        m_id = other.m_id;
        print("T &operator=TestClass(const T&): {}", m_id);
        return *this;
    }
};

int TestClass::counter = 0;

void array_usage()
{

    utils::Array<TestClass> arr(2);

    arr[0] = TestClass{};
    arr[1] = TestClass{};

    // auto other = arr;

    // TestClass a;
    // arr[0] = a;
}

void growing_tree()
{

    tree::Structure str;

    auto root = str.createRoot(0);

    auto n1 = str.addExtraChild(root);

    assert(n1 == str.getChild(root, 0));

    auto n2 = str.addExtraChild(root);

    assert(n1 == str.getChild(root, 0));
    assert(n2 == str.getChild(root, 1));

    auto n3 = str.addExtraChild(root);

    assert(n1 == str.getChild(root, 0));
    assert(n2 == str.getChild(root, 1));
    assert(n3 == str.getChild(root, 2));
}

void run()
{

    growing_tree();

    // array_usage();
}

} // namespace tree_test
} // namespace tests
} // namespace cpprofiler