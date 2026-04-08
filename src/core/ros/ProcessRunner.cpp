#include "core/ros/ProcessRunner.h"

namespace autoviz::ros {

ProcessRunner::ProcessRunner(QObject* parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        emit standardOutputReady(QString::fromLocal8Bit(m_process.readAllStandardOutput()));
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        emit standardErrorReady(QString::fromLocal8Bit(m_process.readAllStandardError()));
    });
    connect(
        &m_process,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &ProcessRunner::processFinished);
}

bool ProcessRunner::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void ProcessRunner::start(const QString& program, const QStringList& arguments, const QString& workingDirectory)
{
    m_process.setWorkingDirectory(workingDirectory);
    m_process.start(program, arguments);
}

void ProcessRunner::stop()
{
    if (isRunning()) {
        m_process.kill();
        m_process.waitForFinished(3000);
    }
}

}  // namespace autoviz::ros
