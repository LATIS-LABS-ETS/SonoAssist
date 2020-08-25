#include "WindowPainter.h"

void WindowPainter::connect_device() {

	// making sure requirements are filled
	if (m_config_loaded && m_sensor_used) {

		// getting thhe target window rect
		m_window_handle = FindWindow(TEXT((*m_config_ptr)["us_window_name"].c_str()), NULL);
		if (m_window_handle != NULL) {
			GetClientRect(m_window_handle, &m_window_rc);
			m_device_connected = true;
		}
	
	}

	emit device_status_change(m_device_connected);

}

void WindowPainter::disconnect_device() {

	// making sure requirements are filled
	if (m_device_connected) {
		m_device_connected = false;
		emit device_status_change(false);
	}

}

void WindowPainter::start_stream() {

	// making sure requirements are filled
	if (m_device_connected && !m_device_streaming) {
	
		// .. 

	}

}

void WindowPainter::stop_stream() {

	if (m_device_streaming) {
		
		// ..

	}

}