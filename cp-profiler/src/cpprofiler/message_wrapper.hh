#ifndef CPPROFILER_MESSAGE_WRAPPER_HH
#define CPPROFILER_MESSAGE_WRAPPER_HH

#include <QMetaType>
#include <memory>

#include "../cpp-integration/message.hpp"

namespace cpprofiler {

/// Helper class which can be registered with Qt's metatype system
///
/// Allows us to pass MessageWrapper references around in multithreaded signals/slots
/// For some reason sending Message* pointers doesn't work
class MessageWrapper
{
public:
    MessageWrapper() = default;
    MessageWrapper(const MessageWrapper &other) = default;
    ~MessageWrapper() = default;

    MessageWrapper(const Message& msg): _msg(msg) {}

    Message& msg() { return _msg; }

private:
    Message _msg;
};

} // namespace cpprofiler

Q_DECLARE_METATYPE(cpprofiler::MessageWrapper);

#endif
