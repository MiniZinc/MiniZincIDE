#pragma once

namespace cpprofiler
{

class Settings
{
public:
    /// delay in ms after receiving a new message
    int receiver_delay = 0;
    int auto_hide_failed = true;
};

} // namespace cpprofiler
