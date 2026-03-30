#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <iostream>
#include "oe_core_bridge.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("OpenEmu Linux Qt6 Smoke Test");
    window.setMinimumSize(400, 200);

    QVBoxLayout *layout = new QVBoxLayout(&window);
    QLabel *label = new QLabel("Qt6 is working!");
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    if (argc > 1) {
        try {
            OpenEmu::CoreBridge bridge(argv[1]);
            QLabel *coreLabel = new QLabel(QString("Successfully loaded core: %1").arg(argv[1]));
            coreLabel->setAlignment(Qt::AlignCenter);
            layout->addWidget(coreLabel);
            std::cout << "Successfully loaded core: " << argv[1] << std::endl;
        } catch (const std::exception& e) {
            QLabel *errorLabel = new QLabel(QString("Failed to load core: %1").arg(e.what()));
            errorLabel->setStyleSheet("color: red;");
            errorLabel->setAlignment(Qt::AlignCenter);
            layout->addWidget(errorLabel);
            std::cerr << "Error: " << e.what() << std::endl;
        }
    } else {
        QLabel *hintLabel = new QLabel("Usage: qt_test <path_to_core_so>");
        hintLabel->setStyleSheet("font-style: italic;");
        hintLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(hintLabel);
    }

    window.show();
    return app.exec();
}
