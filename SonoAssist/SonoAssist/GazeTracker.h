#pragma once

#include "SensorDevice.h"

#include <string>
#include <thread>
#include <fstream>
#include <exception>

#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>

#include <QDebug>

/*
* Class to enable communication with the tobii eye tracker 4C
*/
class GazeTracker : public SensorDevice {

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
		tobii_gaze_point_t get_latest_acquisition(void);
		void set_latest_acquisition(tobii_gaze_point_t data);
		void set_output_file(std::string output_file_path, std::string extension);

		// output file attributes
		std::ofstream m_output_file;

	private:

		// tobii communication vars
		bool m_tobii_api_valid = false;
		tobii_api_t* m_tobii_api = nullptr;
		tobii_device_t* m_tobii_device = nullptr;

		// output file vars
		bool m_output_file_loaded = false;
		std::string m_output_file_str = "";

		// acquisition vars
		tobii_gaze_point_t m_latest_acquisition;

		// streaming vars
		bool m_collect_data = false;
		std::thread m_collection_thread;

};

// helper and call back function prototypes
void url_receiver(char const* url, void* user_data);
void gaze_data_callback(tobii_gaze_point_t const* gaze_point, void* user_data);