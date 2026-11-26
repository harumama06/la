#include "MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QPalette>
#include <QColor>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Game Control Center");
    QApplication::setOrganizationName("GCC");
    app.setStyle("Fusion");
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(30,30,30));
    dark.setColor(QPalette::WindowText, QColor(238,238,238));
    dark.setColor(QPalette::Base, QColor(24,24,24));
    dark.setColor(QPalette::AlternateBase, QColor(42,42,42));
    dark.setColor(QPalette::ToolTipBase, QColor(45,45,45));
    dark.setColor(QPalette::ToolTipText, QColor(240,240,240));
    dark.setColor(QPalette::Text, QColor(238,238,238));
    dark.setColor(QPalette::Button, QColor(45,45,45));
    dark.setColor(QPalette::ButtonText, QColor(238,238,238));
    dark.setColor(QPalette::BrightText, QColor(255,80,80));
    dark.setColor(QPalette::Highlight, QColor(45,125,210));
    dark.setColor(QPalette::HighlightedText, QColor(255,255,255));
    app.setPalette(dark);

    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dataDir);

    MainWindow window;
    window.show();

    return app.exec();
}
