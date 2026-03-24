#include "app/Application.h"
#include <QDebug>
#include <iostream>

int main(int argc, char *argv[])
{
    try {
        Application app(argc, argv);
        if (!app.initialize()) {
            qCritical() << "Failed to initialize application.";
            return 1;
        }
        return app.exec();
    } catch (const std::exception& e) {
        qCritical() << "Exception:" << e.what();
        std::cerr << "Exception: " << e.what() << std::endl;
        return 2;
    } catch (...) {
        qCritical() << "Unknown exception occurred";
        std::cerr << "Unknown exception occurred" << std::endl;
        return 3;
    }
}