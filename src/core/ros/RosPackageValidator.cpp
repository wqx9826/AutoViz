#include "core/ros/RosPackageValidator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

namespace autoviz::ros {
namespace {
QString readPackageName(const QString& packageXmlPath)
{
    QFile file(packageXmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QXmlStreamReader reader(&file);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("name")) {
            return reader.readElementText().trimmed();
        }
    }

    return QString();
}
}  // namespace

RosPackageValidationResult RosPackageValidator::validate(const QString& directoryPath)
{
    RosPackageValidationResult result;

    const QFileInfo dirInfo(directoryPath);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        result.errorMessage = QStringLiteral("所选路径不是有效目录。");
        return result;
    }

    QDir packageDir(dirInfo.absoluteFilePath());
    result.packagePath = packageDir.absolutePath();
    result.packageName = packageDir.dirName();

    result.packageXmlPath = packageDir.filePath(QStringLiteral("package.xml"));
    if (!QFileInfo::exists(result.packageXmlPath)) {
        result.errorMessage = QStringLiteral("缺少 package.xml。");
        return result;
    }

    result.cmakeListsPath = packageDir.filePath(QStringLiteral("CMakeLists.txt"));
    if (!QFileInfo::exists(result.cmakeListsPath)) {
        result.errorMessage = QStringLiteral("缺少 CMakeLists.txt。");
        return result;
    }

    result.msgDirectoryPath = packageDir.filePath(QStringLiteral("msg"));
    const QFileInfo msgDirInfo(result.msgDirectoryPath);
    if (!msgDirInfo.exists() || !msgDirInfo.isDir()) {
        result.errorMessage = QStringLiteral("缺少 msg 目录。");
        return result;
    }

    result.msgFiles = QDir(result.msgDirectoryPath).entryList(
        QStringList() << QStringLiteral("*.msg"),
        QDir::Files,
        QDir::Name);

    result.msgCount = result.msgFiles.size();
    if (result.msgCount <= 0) {
        result.errorMessage = QStringLiteral("msg 目录中未检测到 .msg 文件。");
        return result;
    }

    const QString packageNameFromXml = readPackageName(result.packageXmlPath);
    if (!packageNameFromXml.isEmpty()) {
        result.packageName = packageNameFromXml;
    }

    result.isValid = true;
    return result;
}

}  // namespace autoviz::ros
