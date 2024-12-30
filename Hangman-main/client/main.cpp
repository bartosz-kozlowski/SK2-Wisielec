#include "mywidget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MyWidget w;
    w.setWindowTitle("Wisielec");
    w.show();
    w.setFixedSize(1573, 597);
    return a.exec();
}
