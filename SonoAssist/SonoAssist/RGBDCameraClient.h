#pragma once

#include "SensorDevice.h"

#include <string>
#include <thread>
#include <chrono>

#include <opencv2/opencv.hpp>
#include <librealsense2/rs.hpp>

#include <QImage>
#include <QDebug>

#define RGB_FPS 30
#define RGB_WIDTH 1920 
#define RGB_HEIGHT 1080 
#define DEPTH_FPS 30
#define DEPTH_WIDTH 1280 
#define DEPTH_HEIGHT 720
#define N_SKIP_FRAMES 30

// resize factor to fit a target display of (360 x 640) px
#define CAMERA_DISPLAY_RESIZE_FACTOR 3
#define CAMERA_DISPLAY_THREAD_DELAY_MS 150

/*
* Class to enable communication with the Intel Realsens D435 camera
*/
class RGBDCameraClient : public SensorDevice {

	Q_OBJECT

	public:

		~RGBDCameraClient();

		// SensorDevice interface functions
		void stop_stream(void);
		void start_stream(void);
		void connect_device(void);
		void disconnect_device(void);
		void set_output_file(std::string output_folder);

		// threaded data collection function
		void collect_camera_data(void);

	signals:
		void new_video_frame(QImage image);

	private:

		// camera communication vars
		std::unique_ptr <rs2::config> m_camera_cfg_p;
		std::unique_ptr< rs2::pipeline> m_camera_pipe_p;

		// streaming vars
		bool m_collect_data = false;
		std::thread m_collection_thread;

		// output file vars
		bool m_output_file_loaded = false;
		std::string m_camera_output_file_str = "";

};