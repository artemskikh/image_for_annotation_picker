#include <QApplication>
#include "MainWindow.h"
#include "Logger.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Initialize logging
    Logger::initialize();

    app.setApplicationName("Image Annotation Picker");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ImageAnnotationPicker");

    LOG_INFO("Starting Image Annotation Picker v1.0.0");

    MainWindow window;
    window.show();

    LOG_INFO("Main window displayed");

    int result = app.exec();

    LOG_INFO("Application exiting with code: {}", result);
    return result;
}
