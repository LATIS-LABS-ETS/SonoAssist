#include "SonoAssist.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	SonoAssist w;
	w.show();
	return a.exec();
}