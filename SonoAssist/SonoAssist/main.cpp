
#include "SonoAssist.h"

#include <QDebug>
#include <QtWidgets/QApplication>

#ifdef _WIN32
	#include <Winsock2.h>
#endif _WIN32

int main(int argc, char *argv[]){

	int return_code;

    // defining the Qt application and main window
    QApplication a(argc, argv);
	SonoAssist w;

	// requirement for the use of redis
	#ifdef _WIN32
		//! Windows netword DLL init
		WORD version = MAKEWORD(2, 2);
		WSADATA data;
		if (WSAStartup(version, &data) != 0) {
			std::cerr << "WSAStartup() failure" << std::endl;
			return -1;
		}
	#endif /* _WIN32 */

	// running the application
	w.show();
	return_code = a.exec();

	// requirement for the use of redis
	#ifdef _WIN32
		WSACleanup();
	#endif /* _WIN32 */

	return return_code;

}