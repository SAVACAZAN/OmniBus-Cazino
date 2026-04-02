#include <QApplication>
#include <QFile>
#include "app/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("OmniBus Casino");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("OmniBus");

    // Load dark theme stylesheet
    QFile qss(":/styles/dark_theme.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
        qss.close();
    }

    MainWindow window;
    window.show();

    return app.exec();
}
