
#include "SonoAssist.h"
#include "MetaWearBluetoothClient.h"

#include <QtWidgets/QApplication>

int main(int argc, char *argv[]){
	
	// Qt GUI stuff
	QApplication a(argc, argv);
	SonoAssist w;
	w.show();
	return a.exec();

}

