/*
* STEP #1: Implement your custom sensor class as a child class of the SensorDevice abstract class.
*
* When implementing the virtual methods, keep in mind the following :
*
*	 1) The call to the "start_stream" function must be non-blocking, meaning the data collection process has to be
*	   done in another thread created and maintained by your custom class.
*
*	 2) The "connect_to_redis", "write_to_redis" and "disconnect_from_redis" methods defined in the SensorDevice class
*	   may have to be overridden depending on your specific needs.
*/

#ifndef SENSOREXAMPLE_H
#define SENSOREXAMPLE_H

#include "SensorDevice.h"

#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <stdlib.h> 

class SensorExample : public SensorDevice {

	Q_OBJECT

	public:

		SensorExample(int device_id, std::string device_description, std::string redis_state_entry, std::string log_file_path)
			: SensorDevice(device_id, device_description, redis_state_entry, log_file_path) {};

		// SensorDevice interface functions
		void stop_stream(void);
		void start_stream(void);
		void connect_device(void);
		void disconnect_device(void);
		void set_output_file(std::string output_folder);

		// threaded collection function
		void collect_sensor_data(void);

	protected:

		// thread vars
		bool m_collect_data = false;
		std::thread m_collection_thread;

		// output file vars
		bool m_output_file_loaded = false;
		std::ofstream m_output_data_file;
		std::string m_output_data_file_str;

		// redis entry
		std::string m_redis_entry;

};

#endif