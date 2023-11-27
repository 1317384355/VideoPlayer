#include <QAPPlication>
#include "demo.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    CMediaDialog w;
    w.show();
    return 0;
}