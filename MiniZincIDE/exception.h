#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <QException>

class Exception : public QException
{
public:
    Exception(const QString& _msg) : msg(_msg) {}

    const QString& message(void) const { return msg; }

    void raise() const override { throw *this; }
    Exception *clone() const override { return new Exception(*this); }

protected:
    QString msg;
};

class InternalError : public Exception
{
public:
    InternalError(const QString& _msg) : Exception(_msg) {}

    void raise() const override { throw *this; }
    Exception *clone() const override { return new InternalError(*this); }
};

class ProcessError : public Exception
{
public:
    ProcessError(const QString& _msg) : Exception(_msg) {}

    void raise() const override { throw *this; }
    Exception *clone() const override { return new ProcessError(*this); }
};

class ProjectError : public Exception
{
public:
    ProjectError(const QString& _msg) : Exception(_msg) {}

    void raise() const override { throw *this; }
    Exception *clone() const override { return new ProjectError(*this); }
};

class MoocError : public Exception
{
public:
    MoocError(const QString& _msg) : Exception(_msg) {}

    void raise() const override { throw *this; }
    Exception *clone() const override { return new MoocError(*this); }
};

class ConfigError : public Exception
{
public:
    ConfigError(const QString& _msg) : Exception(_msg) {}

    void raise() const override { throw *this; }
    Exception *clone() const override { return new ConfigError(*this); }
};

class DriverError : public Exception
{
public:
    DriverError(const QString& _msg) : Exception(_msg) {}

    void raise() const override { throw *this; }
    Exception *clone() const override { return new DriverError(*this); }
};

class FileError : public Exception
{
public:
    FileError(const QString& _msg) : Exception(_msg) {}

    void raise() const override { throw *this; }
    Exception *clone() const override { return new FileError(*this); }
};

class ServerError : public Exception
{
public:
    ServerError(const QString& _msg) : Exception(_msg) {}

    void raise() const override { throw *this; }
    Exception *clone() const override { return new ServerError(*this); }
};

#endif // EXCEPTION_H
