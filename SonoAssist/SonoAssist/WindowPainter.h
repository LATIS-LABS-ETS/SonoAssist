#pragma once

#include <string>
#include <thread>
#include <chrono>

#include <QDebug>

#include "SensorDevice.h"


class WindowPainter : public SensorDevice {

	public:
		
		void stop_stream(void);
		void start_stream(void);
		void connect_device(void);
		void disconnect_device(void);

	protected:

		RECT m_window_rc;
		HWND m_window_handle;
		std::string m_window_name;
		
};

