#include "SensorDevice.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// getters and setters

bool SensorDevice::get_sensor_used() const {
	return m_sensor_used;
}

void SensorDevice::set_sensor_used(bool state) {
	m_sensor_used = state;
}

bool SensorDevice::get_connection_status() const {
	return m_device_connected;
}

void SensorDevice::set_connection_status(bool state) {
	m_device_connected = state;
}

bool SensorDevice::get_stream_status() const {
	return m_device_streaming;
}

void SensorDevice::set_stream_status(bool state) {
	m_device_streaming = state;
}

bool SensorDevice::get_stream_preview_status() const {
	return m_stream_preview;
}

void SensorDevice::set_stream_preview_status(bool state) {
	if (m_device_connected && !m_device_streaming) {
		m_stream_preview = state;
	}
}

void SensorDevice::set_configuration(std::shared_ptr<config_map> config_ptr) {
	m_config_ptr = config_ptr;
	m_config_loaded = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// redis methods

/**
* Connects to redis and creates a list with a name specified by the (m_redis_entry) variable
*/
void SensorDevice::connect_to_redis(void) {

	// connecting to redis and initializing the data list
	m_redis_client.connect();
	m_redis_client.del(std::vector<std::string>({m_redis_entry}));
	m_redis_client.rpush(m_redis_entry, std::vector<std::string>({ "" }));
	m_redis_client.sync_commit();

}

void SensorDevice::disconnect_from_redis(void) {
	m_redis_client.disconnect();
}


/**
* Once every (m_redis_rate_div) function calls, the provided data is appended to the
* (m_redis_entry) list
*/
void SensorDevice::write_to_redis(std::string data_str){

	// writing to redis
	if (m_redis_client.is_connected()) {
		if ((m_redis_data_count % m_redis_rate_div) == 0) {
			m_redis_client.rpushx(m_redis_entry, data_str);
			m_redis_client.sync_commit();
			m_redis_data_count = 1;
		}
		else {
			m_redis_data_count++;
		}
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// helpers

/**
* Generates a milliseconds precision timestamp
* 
* @returns string of millisecond count since epoch
*/
std::string SensorDevice::get_millis_timestamp(void) const {

	auto time_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	return std::to_string(time_stamp);

}