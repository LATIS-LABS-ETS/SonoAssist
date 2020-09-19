#pragma once

#include "main.h"
#include "SensorDevice.h"

#include <string>
#include <thread>
#include <chrono>
#include <fstream>

#include <QDebug>
#include <QImage>

#include <opencv2/opencv.hpp>

// SR_PREVIEW_RESIZE_FACTOR : to fit a (360 x 640) px display
#define SR_PREVIEW_RESIZE_FACTOR 3

#define SCREEN_CAPTURE_FPS 10
#define CAPTURE_DISPLAY_THREAD_DELAY_MS 150

class ScreenRecorder : public SensorDevice {

	Q_OBJECT

	public:
		
		ScreenRecorder();

		// SensorDevice interface functions
		void stop_stream(void);
		void start_stream(void);
		void connect_device(void);
		void disconnect_device(void);
		void set_output_file(std::string output_folder);
		
		// threaded collection function
		void collect_window_captures(void);

		// utility functions
		void hwnd2mat(void);
		void get_screen_dimensions(int&, int&) const;

	signals:
		void new_window_capture(QImage image);

	protected:

		// window capture vars
		RECT m_window_rc;
		HWND m_window_handle;
		int m_resized_img_width = 0;
		int m_resized_img_height = 0;
		// image handling containers
		QImage m_output_img;
		cv::Mat m_capture_mat;
		cv::Mat m_output_img_mat;
		cv::Mat m_capture_cvt_mat;
		
		// thread vars
		bool m_collect_data = false;
		std::thread m_collection_thread;

		// output file vars
		bool m_output_file_loaded = false;
		std::ofstream m_output_index_file;
		std::string m_output_index_file_str;
		std::string m_output_video_file_str;

		// video output vars
		std::unique_ptr<cv::VideoWriter> m_video;
		
};

