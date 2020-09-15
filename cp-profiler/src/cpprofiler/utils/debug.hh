#pragma once

#include <iostream>
#include <vector>
#include <QString>
#include <QDebug>
#include <sstream>

namespace cpprofiler
{

inline std::ostream &operator<<(std::ostream &os, const QString &str)
{
    return os << str.toStdString();
}

template <typename T>
std::string to_string(const std::vector<T> &vec)
{
    if (vec.size() == 0)
    {
        return "[]";
    }

    std::ostringstream oss;
    oss << "[";
    for (auto i = 0u; i < vec.size() - 1; ++i)
    {
        oss << vec[i] << ",";
    }
    oss << vec.back() << "]";
    return oss.str();
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec)
{
    return os << to_string(vec);
}

inline std::string format(const char *temp)
{
    return temp;
}

template <typename T, typename... Types>
std::string format(const char *temp, T value, Types... args)
{
    std::ostringstream oss;
    for (; *temp != '\0'; ++temp)
    {

        if (*temp == '{')
        {
            oss << value;
            oss << format(temp + 2, args...);
            break;
        }
        oss << *temp;
    }

    return oss.str();
}

template <typename... Types>
void print(const char *temp, Types... args)
{
    std::cerr << format(temp, args...) << std::endl;
}

} // namespace cpprofiler