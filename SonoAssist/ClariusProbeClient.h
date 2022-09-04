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

#include "main.h"
#include "SensorDevice.h"

#include <listen/listen.h>

#include <string>
#include <vector>
#include <fstream>

#include <opencv2/opencv.hpp>

#include <QtGui/QtGui>
#include <QStandardPaths>
#include <QtWidgets/QApplication>

#define CLARIUS_PREVIEW_IMG_WIDTH 640
#define CLARIUS_PREVIEW_IMG_HEIGHT 360
#define CLARIUS_DEFAULT_IMG_WIDTH 640
#define CLARIUS_DEFAULT_IMG_HEIGHT 480
#define CLARIUS_NORMAL_DEFAULT_WIDTH 640
#define CLARIUS_NORMAL_DEFAULT_HEIGHT 480

#define CLARIUS_VIDEO_FPS 20

// required by the ClariusProbeClient
class SonoAssist;

/*
* Class to enable communication with a Clarius ultrasound probe
* 
* The output IMU file also serves as a index (timestamp) for US images since IMU data
* is received with the probe images.
*/
class ClariusProbeClient : public SensorDevice {

    Q_OBJECT


    public:

		ClariusProbeClient(int device_id, const std::string& device_description, 
			const std::string& redis_state_entry, const std::string& log_file_path)
			: SensorDevice(device_id, device_description, redis_state_entry, log_file_path) {};

        // SensorDevice interface functions
        void stop_stream(void);
        void start_stream(void);
        void connect_device(void);
        void disconnect_device(void);
        void set_output_file(const std::string& output_folder_path);

		void set_udp_port(int port);
		void write_output_data(void);

	public:
		
		// ouput image dimensions (accessed from callback)
		int m_out_img_width = CLARIUS_NORMAL_DEFAULT_WIDTH;
		int m_out_img_height = CLARIUS_NORMAL_DEFAULT_HEIGHT;

		// image handling vars (accessed from callback)
		QImage m_output_img;
		cv::Mat m_cvt_mat;
		cv::Mat m_input_img_mat;
		cv::Mat m_output_img_mat;
		cv::Mat m_video_img_mat; 

		// imu check var
		bool m_imu_missing = false;

		// output vars (accessed from callback)
		std::string m_onboard_time;
		std::string m_display_time;
		std::string m_reception_time;
		std::vector<std::string> m_imu_data;
		
		// display resource handling vars (accessed from callback)
		std::atomic<bool> m_display_locked = true;
		std::atomic<bool> m_handler_locked = false;

	private:
		// helper functions
		void initialize_img_handling(void);
		void configure_img_acquisition(void);

	private:

		// output vars
		bool m_output_file_loaded = false;
		std::string m_output_imu_file_str;
		std::string m_output_video_file_str;

		// output writing vars (accessed from callback)
		bool m_writing_ouput = false;
		std::ofstream m_output_imu_file;
		cv::VideoWriter m_video;

		// custom redis entry names
		std::string m_redis_imu_entry;
		std::string m_redis_img_entry;

		int m_udp_port = 0;

	signals:
		void no_imu_data(void);
		void new_us_image(QImage image);

};