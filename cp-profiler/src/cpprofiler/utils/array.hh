#ifndef CPPROFILER_UTILS_ARRAY_HH
#define CPPROFILER_UTILS_ARRAY_HH

#include <stdlib.h>
#include <initializer_list>
#include <algorithm>

namespace cpprofiler
{
namespace utils
{

template <typename T>
class Array
{

    int m_size;
    T *m_data;

  public:
    /// Creates an uninitialized array of size `size`
    Array(int size);
    Array(std::initializer_list<T> init_list);
    Array();
    ~Array();

    Array(const Array &other);
    Array &operator=(const Array &other);

    Array(Array &&other);

    T &operator[](int pos);

    const T &operator[](int pos) const;

    int size() const;
};

template <typename T>
Array<T> &Array<T>::operator=(const Array<T> &other)
{
    free(m_data);
    m_size = other.m_size;
    m_data = static_cast<T *>(malloc(m_size * sizeof(T)));

    for (auto i = 0; i < m_size; ++i)
    {
        m_data[i] = other[i];
    }

    return *this;
}

template <typename T>
Array<T>::Array(int size) : m_size(size)
{
    m_data = static_cast<T *>(malloc(size * sizeof(T)));
}

template <typename T>
Array<T>::Array(std::initializer_list<T> init_list)
    : m_size(init_list.size())
{
    m_data = static_cast<T *>(malloc(m_size * sizeof(T)));

    int i = 0;
    for (auto &&el : init_list)
    {
        m_data[i] = el;
        ++i;
    }
}

template <typename T>
Array<T>::Array() : m_size(0)
{
}

template <typename T>
Array<T>::Array(const Array &other) : m_size(other.size())
{
    m_data = static_cast<T *>(malloc(m_size * sizeof(T)));

    for (auto i = 0; i < m_size; ++i)
    {
        m_data[i] = other[i];
    }
}

template <typename T>
Array<T>::Array(Array &&other) : m_size(other.size())
{
    m_data = other.m_data;
    other.m_size = 0;
    other.m_data = nullptr;
}

template <typename T>
Array<T>::~Array()
{
    free(m_data);
}

template <typename T>
T &Array<T>::operator[](int pos)
{
    return m_data[pos];
}

template <typename T>
const T &Array<T>::operator[](int pos) const
{
    return m_data[pos];
}

template <typename T>
int Array<T>::size() const
{
    return m_size;
}

} // namespace utils
} // namespace cpprofiler

#endif