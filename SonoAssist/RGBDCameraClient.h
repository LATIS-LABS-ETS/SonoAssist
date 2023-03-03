#pragma once

#include "SensorDevice.h"

#include <string>
#include <thread>
#include <chrono>
#include <fstream>

#include <opencv2/opencv.hpp>
#include <librealsense2/rs.hpp>

#include <QImage>

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

/**
* Class to enable communication with the Intel Realsens D435 camera
*/
class RGBDCameraClient : public SensorDevice {

	Q_OBJECT

	public:

		RGBDCameraClient(int device_id, const std::string& device_description, 
			const std::string& redis_state_entry, const std::string& log_file_path): 
			SensorDevice(device_id, device_description, redis_state_entry, log_file_path) {};
		~RGBDCameraClient();

		void stop_stream(void) override;
		void start_stream(void) override;
		void connect_device(void) override;
		void disconnect_device(void) override;
		void set_output_file(const std::string& output_folder) override;

	private:
		
		/**
		* Collects frames from the camera stream to saves and make them available to the main window.
		* This function is meant to be executed in a seperate thread.
		*/
		void collect_camera_data(void);

		// camera communication vars
		rs2::config m_camera_cfg;
		rs2::pipeline m_camera_pipe;

		// streaming vars
		bool m_collect_data = false;
		std::thread m_collection_thread;

		// output file vars
		bool m_output_file_loaded = false;
		std::ofstream m_output_index_file;
		std::string m_output_file_str = "";
		std::string m_output_index_file_str = "";

	signals:
		void new_video_frame(QImage image);

};