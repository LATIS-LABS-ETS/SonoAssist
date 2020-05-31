#include "SensorDevice.h"

void SensorDevice::set_configuration(std::shared_ptr<config_map> config_ptr) {
	m_config_ptr = config_ptr;
	m_config_loaded = true;
}

bool SensorDevice::get_connection_status() {
	return m_device_connected;
}

void SensorDevice::set_connection_status(bool state) {
	m_device_connected = state;
}

bool SensorDevice::get_stream_status() {
	return m_device_streaming;
}

void SensorDevice::set_stream_status(bool state) {
	m_device_streaming = state;
}
