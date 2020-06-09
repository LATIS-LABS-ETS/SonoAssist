
#include "SonoAssist.h"
#include "MetaWearBluetoothClient.h"

#include <QtWidgets/QApplication>

#ifdef _WIN32
	#include <Winsock2.h>
#endif _WIN32

int main(int argc, char *argv[]){

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

	// Qt GUI stuff
	QApplication a(argc, argv);
	SonoAssist w;
	w.show();
	return a.exec();

	// requirement for the use of redis
	#ifdef _WIN32
		WSACleanup();
	#endif /* _WIN32 */

}
