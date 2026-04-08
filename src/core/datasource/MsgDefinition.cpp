#include "core/datasource/MsgDefinition.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace autoviz::datasource {

namespace {
QString inferPackageName(const QFileInfo& fileInfo)
{
    const QDir parentDir = fileInfo.dir();
    if (parentDir.dirName() == QStringLiteral("msg")) {
        QDir packageDir = parentDir;
        if (packageDir.cdUp()) {
            return packageDir.dirName();
        }
    }
    return QString();
}
}  // namespace

bool MsgDefinition::isValid() const
{
    return !filePath.isEmpty() && !messageName.isEmpty() && parsed && parseError.isEmpty();
}

QString MsgDefinition::displayName() const
{
    return packageName.isEmpty() ? messageName : QStringLiteral("%1/%2").arg(packageName, messageName);
}

MsgDefinition MsgDefinition::fromFile(const QString& path)
{
    MsgDefinition definition;
    definition.filePath = path;

    const QFileInfo fileInfo(path);
    definition.messageName = fileInfo.completeBaseName();
    definition.packageName = inferPackageName(fileInfo);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        definition.parsed = false;
        definition.parseError = QStringLiteral("无法读取消息文件。");
        return definition;
    }

    QTextStream stream(&file);
    definition.rawText = stream.readAll();
    definition.parsed = !definition.rawText.isEmpty() && !definition.messageName.isEmpty();

    if (!definition.parsed) {
        definition.parseError = QStringLiteral("消息文件内容为空或消息名无效。");
    }

    return definition;
}

}  // namespace autoviz::datasource
