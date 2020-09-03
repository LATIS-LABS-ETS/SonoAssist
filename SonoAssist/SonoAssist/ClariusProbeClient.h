#pragma once

#ifdef __clang__
	#pragma clang diagnostic push
#elif _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 4251)
	#pragma warning(disable: 4127)
	#pragma warning(disable: 4512)
	#pragma warning(disable: 4510)
	#pragma warning(disable: 4610)
	#pragma warning(disable: 4458)
	#pragma warning(disable: 4800)
#endif

#ifdef __clang__
	#pragma clang diagnostic pop
#elif _MSC_VER
	#pragma warning(pop)
#endif

#include "SensorDevice.h"

#include <listen/listen.h>

#include <string>
#include <Vector>

#include <QDebug>
#include <QtGui/QtGui>
#include <QStandardPaths>
#include <QtWidgets/QApplication>

#define IMAGE_EVENT static_cast<QEvent::Type>(QEvent::User + 1)

/*
* Class to enable communication with a Clarius ultrasound probe
*/
class ClariusProbeClient : public SensorDevice {

    Q_OBJECT

    public:

        // SensorDevice interface functions
        void stop_stream(void);
        void start_stream(void);
        void connect_device(void);
        void disconnect_device(void);
        void set_output_file(std::string output_folder) {};

};