#include "SensorDevice.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// constructor && destructor

SensorDevice::SensorDevice(int device_id, const std::string& device_description, 
	const std::string& redis_state_entry, const std::string& log_file_path):
	m_device_id(device_id), m_device_description(device_description), m_redis_state_entry(redis_state_entry){

	if (log_file_path != "") {
		m_log_file.open(log_file_path, std::fstream::app);
	}
	
}

SensorDevice::~SensorDevice() {
	m_log_file.close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// getters and setters

int SensorDevice::get_device_id(void) const {
	return m_device_id;
}

std::string SensorDevice::get_device_description(void) const {
	return m_device_description;
}

bool SensorDevice::get_sensor_used(void) const {
	return m_sensor_used;
}

void SensorDevice::set_sensor_used(bool state) {
	m_sensor_used = state;
}

bool SensorDevice::get_pass_through(void) const {
	return m_pass_through;
}

void SensorDevice::set_pass_through(bool state) {
	m_pass_through = state;
}

bool SensorDevice::get_connection_status(void) const {
	return m_device_connected;
}

void SensorDevice::set_connection_status(bool state) {
	m_device_connected = state;
}

bool SensorDevice::get_stream_status(void) const {
	return m_device_streaming;
}

void SensorDevice::set_stream_status(bool state) {
	m_device_streaming = state;
}

bool SensorDevice::get_stream_preview_status(void) const {
	return m_stream_preview;
}

void SensorDevice::set_stream_preview_status(bool state) {
	if (m_device_connected && !m_device_streaming) {
		m_stream_preview = state;
	}
}

bool SensorDevice::get_redis_state(void) const {
	return m_redis_state;
}

void SensorDevice::set_configuration(std::shared_ptr<config_map> config_ptr) {

	m_config_ptr = config_ptr;
	
	// getting the redis status (is the device expected to connect to redis)
	try {
		m_redis_state = (*m_config_ptr)[m_redis_state_entry] == "true";
	} catch (...) {
		m_redis_state = false;
		write_debug_output("Failed to load redis status from config");
	}
	
	m_config_loaded = true;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// redis methods

void SensorDevice::connect_to_redis(const std::vector<std::string>&& redis_entries) {

	try {
		
		// creating a new redis connection
		sw::redis::ConnectionOptions connection_options;
		connection_options.host = REDIS_ADDRESS;
		connection_options.port = REDIS_PORT;
		connection_options.socket_timeout = std::chrono::milliseconds(REDIS_TIMEOUT);
		m_redis_client_p = std::make_unique<sw::redis::Redis>(connection_options);

		// clearing the sepcified entries
		for (auto i = 0; i < redis_entries.size(); i++)
			m_redis_client_p->del(redis_entries[i]);

		m_redis_state = true;

	} catch (...) {
		m_redis_state = false;
		write_debug_output("Failed to connect to redis");
	}

}

void SensorDevice::disconnect_from_redis(void) {
	m_redis_client_p = nullptr;
}


/*
* Once every (m_redis_rate_div) function call, the provided string is appended to the specified redis list
*/
void SensorDevice::write_str_to_redis(const std::string& redis_entry, std::string data_str) {

	if (m_redis_state && m_redis_client_p != nullptr) {
	
		try {

			if ((m_redis_data_count % m_redis_rate_div) == 0) {
				m_redis_client_p->rpush(redis_entry, {data_str});
				m_redis_data_count = 1;
			} else {
				m_redis_data_count++;
			}

		} catch (...) {
			m_redis_state = false;
			write_debug_output("Failed to write (string) to redis");
		};

	}

}


/*
* Once every (m_redis_rate_div) function call, the provided image overwrites the specified redis entry
*/
void SensorDevice::write_img_to_redis(const std::string& redis_entry, const cv::Mat& img) {

	if (m_redis_state && m_redis_client_p != nullptr) {

		try {
		
			if ((m_redis_data_count % m_redis_rate_div) == 0) {
				size_t mat_byte_size = img.step[0] * img.rows;
				m_redis_client_p->set(redis_entry, std::string((char*)img.data, mat_byte_size));
				m_redis_data_count = 1;
			} else {
				m_redis_data_count++;
			}
		
		} catch (...) {
			m_redis_state = false;
			write_debug_output("Failed to write (image) to redis");
		}

	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// helpers

/**
* Generates a micro second precision timestamp
* 
* @returns string of micro second count since epoch
*/
std::string SensorDevice::get_micro_timestamp(void) {

	auto time_stamp = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::high_resolution_clock::now().time_since_epoch()).count();

	return std::to_string(time_stamp);

}

/*
* Writes debug output to QDebug (the debug console) and the debug output window
*/
void SensorDevice::write_debug_output(const QString& debug_str) {

	QString out_str = QString::fromUtf8(m_device_description.c_str()) + " - " + debug_str;

	// sending msg to the application and vs debug windows
	qDebug() << "\n" + out_str;
	emit debug_output(out_str);

	// logging the message
	m_log_file << out_str.toStdString() << std::endl;

}