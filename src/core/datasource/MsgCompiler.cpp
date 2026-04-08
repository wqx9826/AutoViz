#include "core/datasource/MsgCompiler.h"

namespace autoviz::datasource {

bool MsgCompiler::compilePackage(const RosMsgPackage& package)
{
    Q_UNUSED(package);
    m_lastError = QStringLiteral("当前版本尚未实现真实消息编译，仅完成包扫描与配置绑定。");
    return false;
}

QString MsgCompiler::lastError() const
{
    return m_lastError;
}

}  // namespace autoviz::datasource
