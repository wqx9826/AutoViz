#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

#include "app/MainWindow.h"
#include "ui/theme/UiScaleManager.h"

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    QApplication::setApplicationName("AutoViz");
    QApplication::setOrganizationName("AutoViz");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/assets/autoviz_icon.png")));
    auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    scale.initialize();
    QApplication::setFont(scale.font(scale.fontSizeNormal()));

    MainWindow window;
    window.show();

    return app.exec();
}
