#include "SonoAssist.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[]){

	int return_code;

    QApplication a(argc, argv);
	SonoAssist w;

	w.show();
	return_code = a.exec();

	return return_code;

}