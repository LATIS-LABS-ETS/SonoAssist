#include "main.h"
#include "SonoAssist.h"

#ifdef _MEASURE_US_IMG_RATES_

// defining execution time measurement vars (US images)
int n_clarius_frames = 0, n_sc_frames = 0;
std::vector<long long> input_img_stamps(N_US_FRAMES, 0);
long long clarius_start, clarius_stop, sc_start, sc_stop;

#endif /*_MEASURE_US_IMG_RATES_*/

// redis requirement
#ifdef _WIN32

#include <Winsock2.h>

#endif /*_WIN32*/

int main(int argc, char *argv[]){

	int return_code;

    // defining the Qt application and main window
    QApplication a(argc, argv);
	SonoAssist w;

// redis requirement
#ifdef _WIN32

	//! Windows netword DLL init
	WORD version = MAKEWORD(2, 2);
	WSADATA data;
	if (WSAStartup(version, &data) != 0) {
		std::cerr << "WSAStartup() failure" << std::endl;
		return -1;
	}

#endif /*_WIN32*/

	// running the application
	w.show();
	return_code = a.exec();

#ifdef _MEASURE_US_IMG_RATES_

	double avg_fps;
	QString measurement_title_str;
	
	// defining title and calculating avg fps
	if (n_clarius_frames > 0) {
		measurement_title_str = "Clarius probe timing info (FPS) : ";
		avg_fps = 1 / ((((double)(clarius_stop - clarius_start)) / MS_PER_S) / N_US_FRAMES);
	} else {
		measurement_title_str = "Screen recorder timing info (FPS) : ";
		avg_fps = 1 / ((((double)(sc_stop - sc_start)) / MS_PER_S) / N_US_FRAMES);
	}

	qDebug() << "\n\n" << measurement_title_str;
	qDebug() << "\nAVG FPS : " << avg_fps << "\n";
	qDebug() << "\nSingle measurements : \n";
	
	// displaying the FPS measurement at every step
	double fps_measurement;
	for (int i(0); i < N_US_FRAMES-1; i++) {
		fps_measurement = 1 / (((double)(input_img_stamps[i + 1] - input_img_stamps[i])) / MS_PER_S);
		qDebug() << i << " : " << fps_measurement << "\n";
	}

#endif /*_MEASURE_US_IMG_RATES_*/

// redis requirement
#ifdef _WIN32

	WSACleanup();

#endif /*_WIN32*/

	return return_code;

}