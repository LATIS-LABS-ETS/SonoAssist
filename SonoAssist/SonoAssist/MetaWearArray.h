#pragma once

#include "SensorDevice.h"
#include "MetaWearBluetoothClient.h"

#include <string>
#include <chrono>
#include <vector>
#include <memory>


class MetaWearArray : public SensorDevice {

	Q_OBJECT

	public:

		MetaWearArray(int device_id, std::string device_description, std::string redis_state_entry, std::string log_file_path)
			: SensorDevice(device_id, device_description, redis_state_entry, log_file_path) {
	
			m_clients.push_back(std::make_shared<MetaWearBluetoothClient>(m_clients.size(), "External IMU #1", "ext_imu_to_redis", log_file_path));
			m_clients.push_back(std::make_shared<MetaWearBluetoothClient>(m_clients.size(), "External IMU #2", "ext_imu_to_redis", log_file_path));

		};

		// interface functions
		void stop_stream(void) {};
		void start_stream(void) {};
		void connect_device(void);
		void disconnect_device(void);
		void set_output_file(std::string output_folder);

	private:
		std::vector<std::shared_ptr<MetaWearBluetoothClient>> m_clients;

};

