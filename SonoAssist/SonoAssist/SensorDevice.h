#pragma once

#include <map>
#include <string>
#include <memory>
#include <chrono>

#include <cpp_redis/cpp_redis>

#include <QObject>

typedef std::map<std::string, std::string> config_map;

/*
* Interface class for the implementation of sensor devices
*
* Classes which communicate with sesnors to pull data from them must implement this interface.
* It provides practical functions and public functions which will allow simple integration to the UI.
*/
class SensorDevice : public QObject {

	Q_OBJECT

	public : 

		// status getters and setters
		bool get_sensor_used(void) const;
		bool get_stream_status(void) const;
		bool get_connection_status(void) const;
		void set_sensor_used(bool state);
		void set_stream_status(bool state);
		void set_connection_status(bool state);
		void set_configuration(std::shared_ptr<config_map> config_ptr);

		// redis communication functions
		void connect_to_redis(void);
		void disconnect_from_redis(void);
		void write_to_redis(std::string data_str);

		// helper functions
		std::string get_millis_timestamp(void) const;
		
		// interface
		virtual void stop_stream(void) = 0;
		virtual void start_stream(void) = 0;
		virtual void connect_device(void) = 0;
		virtual void disconnect_device(void) = 0;
		virtual void set_output_file(std::string ouput_folder) = 0;

	signals:
		void device_status_change(bool is_connected);

	protected:

		bool m_sensor_used = false;
		bool m_device_connected = false;
		bool m_device_streaming = false;

		bool m_config_loaded = false;
		std::shared_ptr<config_map> m_config_ptr;

		// redis output attributes
		int m_redis_rate_div = 2;
		int m_redis_data_count = 1;
		std::string m_redis_entry = "";
		cpp_redis::client m_redis_client;

};

