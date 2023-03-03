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

using config_map = std::map<std::string, std::string>;

/*
* Abstract class for the implementation of custom sensor devices.
*
* To be compatible with SonoAssist, classes which acquire data from sensors must be derived from this class
* and implement (at least) the pure virtual functions.
*/
class SensorDevice : public QObject {

	Q_OBJECT

	public: 

		SensorDevice(int device_id,  std::string device_description, 
			std::string redis_state_entry,  std::string log_file_path);
		virtual ~SensorDevice();

		/*******************************************************************************
		* GETTERS & SETTERS
		******************************************************************************/

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

		/*******************************************************************************
		* REDIS METHODS
		******************************************************************************/

		/**
		* Creates a connection to the local running Redis DB and deletes the provided entry identifiers.
		* Note that if there is no running Redis DB, this no connection will be created (m_redis_state=false) and
		* error message will be printed to the console.
		* 
		* \param redis_entries The Redis entries to be deleted / reset.
		*/
		virtual void connect_to_redis(const std::vector<std::string>&& redis_entries = {});
		
		/**
		* Closes the existing Redis connection.
		*/
		virtual void disconnect_from_redis(void);

		/**
		* Once every (m_redis_rate_div) call, the provided data string is appended to the specified redis list.
		* 
		* \param redis_entry The identifier for the Redis list to which the provided string will be appended.
		* \param data_str The string to append.
		*/
		virtual void write_str_to_redis(const std::string& redis_entry, std::string data_str);
		
		/**
		* Once every (m_redis_rate_div) function call, the provided image overwrites the specified redis entry.
		* 
		* \param redis_entry The identifier to the Redis variable to overwrite with the provided image data.
		* \param img The image data.
		*/
		virtual void write_img_to_redis(const std::string& redis_entry, const cv::Mat& img);

		/*******************************************************************************
		* HELPERS
		******************************************************************************/

		/**
		* Generates a micro second precision timestamp
		*
		* \returns string of micro second count since epoch
		*/
		static std::string get_micro_timestamp(void);

		/*******************************************************************************
		* PURE VIRTUAL METHODS
		* When implemented, all of the following methods must be non-bloking
		******************************************************************************/
		
		/**
		* Establishes a connection with the custom sensor.
		*/
		virtual void connect_device(void) = 0;

		/**
		* Ends the connection with the custom sensor.
		*/
		virtual void disconnect_device(void) = 0;

		/**
		* Launches the data acquisition and writing in an other thread.
		* Must be non-blocking.
		*/
		virtual void start_stream(void) = 0;

		/**
		* Ends the data acquisition process.
		*/
		virtual void stop_stream(void) = 0;

		/**
		* Prepares the writing of the output file(s), given the provided output folder path
		* 
		* \param putput_folder The path to the output folder.
		*/
		virtual void set_output_file(const std::string& ouput_folder) = 0;

	protected:

		/*******************************************************************************
		* HELPERS
		******************************************************************************/

		/**
		* Writes debug output to QDebug (the debug console) and the debug output window
		*/
		void write_debug_output(const QString&);

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