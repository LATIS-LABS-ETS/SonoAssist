#pragma once

#include "SensorDevice.h"

#include <string>
#include <thread>
#include <chrono>

#include <QDebug>
#include <QImage>
#include <opencv2/opencv.hpp>

#define CAPTURE_DISPLAY_RESIZE_FACTOR 3
#define CAPTURE_DISPLAY_THREAD_DELAY_MS 150

class ScreenRecorder : public SensorDevice {

	Q_OBJECT

	public:
		
		// SensorDevice interface functions
		void stop_stream(void);
		void start_stream(void);
		void connect_device(void);
		void disconnect_device(void);
		void set_output_file(std::string output_folder) {};

		// threaded collection function
		void collect_window_captures(void);

		// utility functions
		cv::Mat hwnd2mat(void);

	signals:
		void new_window_capture(QImage image);

	protected:

		// window capture vars
		RECT m_window_rc;
		HWND m_window_handle;
		
		// thread vars
		bool m_collect_data = false;
		std::thread m_collection_thread;
};

