#include "utils/Logger.h"

#include <QDebug>

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::setLogHandler(LogHandler handler)
{
    m_handler = std::move(handler);
}

void Logger::clearLogHandler()
{
    m_handler = nullptr;
}

void Logger::info(const QString& message) const
{
    dispatch("信息", message);
}

void Logger::warning(const QString& message) const
{
    dispatch("警告", message);
}

void Logger::error(const QString& message) const
{
    dispatch("错误", message);
}

void Logger::dispatch(const QString& level, const QString& message) const
{
    const QString formatted = QString("[%1] %2").arg(level, message);
    qInfo().noquote() << formatted;

    if (m_handler) {
        m_handler(formatted);
    }
}
