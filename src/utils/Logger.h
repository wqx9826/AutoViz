#pragma once

#include <functional>

#include <QString>

// Lightweight logger for this stage. It can later be replaced by a richer
// logging backend without changing the panels that consume log messages.
class Logger
{
public:
    using LogHandler = std::function<void(const QString&)>;

    static Logger& instance();

    void setLogHandler(LogHandler handler);
    void clearLogHandler();

    void info(const QString& message) const;
    void warning(const QString& message) const;
    void error(const QString& message) const;

private:
    Logger() = default;

    void dispatch(const QString& level, const QString& message) const;

    LogHandler m_handler;
};
