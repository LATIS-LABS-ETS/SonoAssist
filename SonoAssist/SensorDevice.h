#pragma once

#include <map>
#include <string>
#include <memory>
#include <chrono>
#include <fstream>

#include <QDebug>
#include <QObject>

#include <opencv2/opencv.hpp>
#include <sw/redis++/redis++.h>

#define REDIS_TIMEOUT 200
#define REDIS_PORT 6379
#define REDIS_ADDRESS "127.0.0.1"

typedef std::map<std::string, std::string> config_map;

/*
* Interface for the implementation of sensor devices
*
* Classes which communicate with sesnors to pull data from them must implement this interface.
* It provides practical functions and public functions which will allow simple integration to the UI.
*/
class SensorDevice : public QObject {

	Q_OBJECT

	public: 

		SensorDevice(int device_id,  std::string device_description, 
			std::string redis_state_entry,  std::string log_file_path);
		virtual ~SensorDevice();

		int get_device_id(void) const;
		std::string get_device_description(void) const;
		bool get_sensor_used(void) const;
		bool get_pass_through(void) const;
		bool get_stream_status(void) const;
		bool get_connection_status(void) const;
		bool get_stream_preview_status(void) const;
		bool get_redis_state(void) const;
		
		void set_sensor_used(bool state);
		void set_pass_through(bool state);
		void set_stream_status(bool state);
		void set_connection_status(bool state);
		void set_stream_preview_status(bool state);
		void set_configuration(std::shared_ptr<config_map> config_ptr);

		static std::string get_micro_timestamp(void);

		// redis communication functions
		virtual void connect_to_redis(const std::vector<std::string> && = {});
		virtual void disconnect_from_redis(void);
		virtual void write_str_to_redis(const std::string&, std::string);
		virtual void write_img_to_redis(const std::string&, const cv::Mat&);

		// interface methods (virtual)
		// all interface functions must be non-bloking
		virtual void stop_stream(void) = 0;
		virtual void start_stream(void) = 0;
		virtual void connect_device(void) = 0;
		virtual void disconnect_device(void) = 0;
		virtual void set_output_file(const std::string& ouput_folder) = 0;

	protected:
		void write_debug_output(const QString&);

	protected:

		// device identification vars
		int m_device_id;
		std::string m_device_description;

		// sensor status vars
		bool m_sensor_used = false;
		bool m_pass_through = false;
		bool m_stream_preview = false;
		bool m_device_connected = false;
		bool m_device_streaming = false;

		// configs vars
		bool m_config_loaded = false;
		std::shared_ptr<config_map> m_config_ptr;

		// redis output attributes
		bool m_redis_state = false;
		std::string m_redis_state_entry;
		int m_redis_rate_div = 1;
		int m_redis_data_count = 1;
		std::unique_ptr<sw::redis::Redis> m_redis_client_p;
	
		// output writing vars
		std::ofstream m_log_file;
		std::string m_output_folder_path;

	signals:
		void debug_output(QString debug_str);
		void device_status_change(int device_id, bool is_connected);

};