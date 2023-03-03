#pragma once

#include <string>
#include <thread>
#include <fstream>
#include <Windows.h>

#include "SensorDevice.h"

#define OS_NO_KEY 0
#define OS_A_KEY 1
#define OS_D_KEY 2

/*
* Class for the detection of key presses ('A' and 'D' keys for now)
* The key presses can be detected even when the main application window is out of focus or minimized
*/
class OSKeyDetector : public SensorDevice {

	Q_OBJECT

	public:

		OSKeyDetector(int device_id, const std::string& device_description, 
			const std::string& redis_state_entry, const std::string& log_file_path): 
			SensorDevice(device_id, device_description, redis_state_entry, log_file_path) {};

		void stop_stream(void) override;
		void start_stream(void) override;
		void connect_device(void) override;
		void disconnect_device(void) override;
		void set_output_file(const std::string& output_folder) override {};

	private:

		/**
		* Detects and emits key presses (for the 'A' and 'D' keys) to the main window
		* This method is meant to run in a seperate thread
		*/
		void detect_keys(void);

		bool m_listen = false;
		std::thread m_listener_thread;

	signals:
		void key_detected(int key);

};