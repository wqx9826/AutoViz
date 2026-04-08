#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

namespace autoviz::ros {

class ProcessRunner : public QObject
{
    Q_OBJECT

public:
    explicit ProcessRunner(QObject* parent = nullptr);

    bool isRunning() const;
    void start(const QString& program, const QStringList& arguments, const QString& workingDirectory);
    void stop();

signals:
    void standardOutputReady(const QString& text);
    void standardErrorReady(const QString& text);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess m_process;
};

}  // namespace autoviz::ros
