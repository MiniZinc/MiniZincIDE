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

    virtual const char* what(void) const {
      return "Internal error";
    }
};

class ProcessError : public Exception
{
public:
    ProcessError(const QString& _msg) : Exception(_msg) {}

    virtual const char* what(void) const {
      return "Process error";
    }
};

class ProjectError : public Exception
{
public:
    ProjectError(const QString& _msg) : Exception(_msg) {}

    virtual const char* what(void) const {
      return "Project error";
    }
};


class ConfigError : public Exception
{
public:
    ConfigError(const QString& _msg) : Exception(_msg) {}

    virtual const char* what(void) const {
      return "Configuration error";
    }
};

class DriverError : public Exception
{
public:
    DriverError(const QString& _msg) : Exception(_msg) {}

    virtual const char* what(void) const {
      return "MiniZinc driver error";
    }
};

class FileError : public Exception
{
public:
    FileError(const QString& _msg) : Exception(_msg) {}

    virtual const char* what(void) const {
      return "File error";
    }
};

#endif // EXCEPTION_H
