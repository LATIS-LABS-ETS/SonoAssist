#pragma once

#include "SensorDevice.h"

#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <Windows.h>

#include <QImage>
#include <opencv2/opencv.hpp>

// resize factor to fit a target display of (360 x 640) px
#define SR_PREVIEW_RESIZE_FACTOR 3
#define REDIS_RESIZE_FACTOR 2

#define SCREEN_CAPTURE_FPS 20
#define CAPTURE_DISPLAY_THREAD_DELAY_MS 150

class ScreenRecorder : public SensorDevice {

	Q_OBJECT

	public:
		
		ScreenRecorder(int device_id, const std::string& device_description, 
			const std::string& redis_state_entry, const std::string& log_file_path);
		~ScreenRecorder();

		void stop_stream(void) override;
		void start_stream(void) override;
		void connect_device(void) override;
		void disconnect_device(void) override;
		void set_output_file(const std::string& output_folder) override;
	
		cv::Mat get_lastest_acquisition(cv::Rect aoi=cv::Rect(0, 0, 0, 0));
		void get_screen_dimensions(int&, int&) const;

	private:

		/**
		* Collects the most recent screen capture to save it and makes it available to the main window.
		* This function is meant to be executed in a seperate thread.
		*/
		void collect_window_captures(void);

		void initialize_capture(void);

		// window capture vars
		RECT m_window_rc;
		HWND m_window_handle;
		HBITMAP m_hbwindow;
		BITMAPINFOHEADER m_bi;
		HDC m_hwindowDC, m_hwindowCompatibleDC;
		
		// image handling containers
		QImage m_preview_img;
		cv::Mat m_preview_img_mat;
		cv::Mat m_capture_mat;
		cv::Mat m_capture_cvt_mat;
		cv::Mat m_redis_img_mat;

		// thread vars
		bool m_collect_data = false;
		std::thread m_collection_thread;
		std::mutex m_capture_mtx;

		// output file vars
		bool m_output_file_loaded = false;
		std::ofstream m_output_index_file;
		std::string m_output_index_file_str;
		std::string m_output_video_file_str;

		// video output vars
		cv::VideoWriter m_video;

		// redis entry
		std::string m_redis_img_entry;

	signals:
		void new_window_capture(QImage image);
		
};