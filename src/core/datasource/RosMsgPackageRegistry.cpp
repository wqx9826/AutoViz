#include "core/datasource/RosMsgPackageRegistry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace autoviz::datasource {

bool RosMsgPackageRegistry::loadPackage(const QString& directoryPath)
{
    const RosMsgPackage package = buildPackageFromDirectory(directoryPath);
    if (!package.isValid) {
        m_lastError = package.errorMessage;
        return false;
    }

    m_currentPackage = package;
    m_lastError.clear();
    return true;
}

void RosMsgPackageRegistry::unloadCurrentPackage()
{
    m_currentPackage = RosMsgPackage();
    m_lastError.clear();
}

bool RosMsgPackageRegistry::hasLoadedPackage() const
{
    return m_currentPackage.isValid;
}

const RosMsgPackage* RosMsgPackageRegistry::currentPackage() const
{
    return hasLoadedPackage() ? &m_currentPackage : nullptr;
}

QString RosMsgPackageRegistry::lastError() const
{
    return m_lastError;
}

RosMsgPackage RosMsgPackageRegistry::buildPackageFromDirectory(const QString& directoryPath) const
{
    RosMsgPackage package;
    const QFileInfo dirInfo(directoryPath);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        package.errorMessage = QStringLiteral("所选目录不存在或不是有效目录。");
        return package;
    }

    QDir packageDir(directoryPath);
    package.packagePath = packageDir.absolutePath();
    package.packageName = packageDir.dirName();

    package.packageXmlPath = packageDir.filePath(QStringLiteral("package.xml"));
    if (!QFileInfo::exists(package.packageXmlPath)) {
        package.errorMessage = QStringLiteral("未检测到 package.xml。");
        return package;
    }

    package.msgDirectoryPath = packageDir.filePath(QStringLiteral("msg"));
    const QFileInfo msgDirInfo(package.msgDirectoryPath);
    if (!msgDirInfo.exists() || !msgDirInfo.isDir()) {
        package.errorMessage = QStringLiteral("未检测到 msg 目录。");
        return package;
    }

    const QString packageNameFromXml = readPackageNameFromPackageXml(package.packageXmlPath);
    if (!packageNameFromXml.isEmpty()) {
        package.packageName = packageNameFromXml;
    }

    const QStringList msgEntries =
        QDir(package.msgDirectoryPath).entryList(QStringList() << QStringLiteral("*.msg"), QDir::Files, QDir::Name);

    for (const auto& msgEntry : msgEntries) {
        MsgDefinition definition = MsgDefinition::fromFile(QDir(package.msgDirectoryPath).filePath(msgEntry));
        definition.packageName = package.packageName;
        package.msgFiles.push_back(definition);
    }

    package.isValid = true;
    return package;
}

QString RosMsgPackageRegistry::readPackageNameFromPackageXml(const QString& packageXmlPath) const
{
    QFile file(packageXmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QTextStream stream(&file);
    const QString content = stream.readAll();
    QRegularExpression regex(QStringLiteral("<name>\\s*([^<\\s]+)\\s*</name>"));
    const auto match = regex.match(content);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

}  // namespace autoviz::datasource
