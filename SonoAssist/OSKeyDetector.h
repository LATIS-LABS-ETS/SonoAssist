#ifndef OSKEYDETECTOR_H
#define OSKEYDETECTOR_H


#include <string>
#include <thread>
#include <fstream>
#include <Windows.h>

#include "SensorDevice.h"

#define OS_NO_KEY 0
#define OS_A_KEY 1
#define OS_D_KEY 2

class OSKeyDetector : public SensorDevice {

	Q_OBJECT

	public:

		OSKeyDetector(int device_id, std::string device_description, std::string redis_state_entry, std::string log_file_path)
			: SensorDevice(device_id, device_description, redis_state_entry, log_file_path) {};

		// SensorDevice interface functions
		void stop_stream(void);
		void start_stream(void);
		void connect_device(void);
		void disconnect_device(void);
		void set_output_file(std::string output_folder) {};

		// threaded key detection function
		void detect_keys(void);

	signals:
		void key_detected(int key);

	protected:

		// thread vars
		bool m_listen = false;
		std::thread m_listener_thread;

};

#endif