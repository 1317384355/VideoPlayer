#include "demo.h"
#include <QAPPlication>

int main(int argc, char *argv[])
{
    // QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);

    demo w;
    w.show();
    return a.exec();
}
