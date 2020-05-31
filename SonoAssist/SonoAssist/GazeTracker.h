#pragma once

#include "SensorDevice.h"

#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <exception>

#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>

#include <cpp_redis/cpp_redis>

#include <QDebug>

/*
* Class to enable communication with the tobii eye tracker 4C
*
* The "start_stream" method launches acquisition of gaze data in a seperate thread (collect_gaze_data)
*/
class GazeTracker : public QObject, public SensorDevice {

	Q_OBJECT

	public:

		GazeTracker();
		~GazeTracker();

		// SensorDevice interface functions
		void stop_stream(void);
		void start_stream(void);
		void connect_device(void);
		void disconnect_device(void);

		// threaded collection method and callback
		void collect_gaze_data(void);

		// setters and getters
		void set_output_file(std::string output_file_path, std::string extension);

		// output file attributes
		std::ofstream m_output_file;

		// redis output attributes
		int m_redis_rate_div = 2;
		int m_redis_data_count = 1;
		std::string m_redis_entry = "";
		cpp_redis::client m_redis_client;

	signals:
		void device_status_change(bool is_connected);

	private:

		// tobii communication vars
		bool m_tobii_api_valid = false;
		tobii_api_t* m_tobii_api = nullptr;
		tobii_device_t* m_tobii_device = nullptr;

		// output file vars
		bool m_output_file_loaded = false;
		std::string m_output_file_str = "";

		// streaming vars
		bool m_collect_data = false;
		std::thread m_collection_thread;

};

// helper and call back function prototypes
void url_receiver(char const* url, void* user_data);
void gaze_data_callback(tobii_gaze_point_t const* gaze_point, void* user_data);