#include <QAPPlication>
#include "demo.h"

int main(int argc, char *argv[])
{
    // QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    CMediaDialog w;
    w.show();
    return 0;
}