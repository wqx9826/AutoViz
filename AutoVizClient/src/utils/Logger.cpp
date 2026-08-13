#include "utils/Logger.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QTextStream>

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::initializePersistentLog()
{
    const QString fileName =
        QStringLiteral("autoviz-%1.log").arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));
    const QString deployedLogDirectory =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("log"));

    if (QDir().mkpath(deployedLogDirectory)) {
        const QString deployedLogFile = QDir(deployedLogDirectory).filePath(fileName);
        QFile probe(deployedLogFile);
        if (probe.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            probe.close();
            m_logFilePath = deployedLogFile;
            return;
        }
    }

    // 便携包目录不可写时（例如安装在 Program Files）才使用系统本地目录兜底。
    const QString fallbackDirectory = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("logs"));
    if (QDir().mkpath(fallbackDirectory)) {
        m_logFilePath = QDir(fallbackDirectory).filePath(fileName);
    }
}

QString Logger::logFilePath() const
{
    return m_logFilePath;
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

    if (!m_logFilePath.isEmpty()) {
        QFile logFile(m_logFilePath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&logFile);
            stream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                   << ' ' << formatted << '\n';
        }
    }

    if (m_handler) {
        m_handler(formatted);
    }
}
