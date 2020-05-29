#pragma once

#include <map>
#include <string>
#include <memory>

typedef std::map<std::string, std::string> config_map;

/*
* Interface class for the implementation of sensor devices
*
* A sensor device is expected to connect/disconnect to a data source and stream it's contents
* to one or multiple destinations.
*/
class SensorDevice{

	public : 

		// status getters and setters
		bool get_stream_status(void);
		bool get_connection_status(void);
		void set_stream_status(bool state);
		void set_connection_status(bool state);

		// config related functions
		void set_configuration(std::shared_ptr<config_map> config_ptr);

		// connection functions
		virtual void connect_device(void) = 0;
		virtual void disconnect_device(void) = 0;

		// stream functions
		virtual void stop_stream(void) = 0;
		virtual void start_stream(void) = 0;

	protected:

		bool m_device_connected = false;
		bool m_device_streaming = false;

		bool m_config_loaded = false;
		std::shared_ptr<config_map> m_config_ptr;

};

