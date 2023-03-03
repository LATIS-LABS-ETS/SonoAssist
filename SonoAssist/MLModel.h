#pragma once

#include <string>
#include <memory>
#include <vector>
#include <fstream>
#include <exception>

#include<QDebug>
#include <QImage>
#include <QObject>

#undef slots
#include <torch/script.h>
#define slots Q_SLOTS
#include <opencv2/opencv.hpp>
#include <sw/redis++/redis++.h>

#include "SensorDevice.h"

#define MODEL_DISPLAY_WIDTH 1260
#define MODEL_DISPLAY_HEIGHT 720


/*
* Abstract class for the implementation of ML models loaded from torch scripts.
*
* To be compatible with SonoAssist, classes implementing a prediction model must be derived from this class
* and implement (at least) the pure virtual functions.
* Note that modifications to the custom (SensorDevice) classes might be required in order to gain access to
* the relevant sensor data (for the predictions).
*/
class MLModel : public QObject {

	Q_OBJECT

	public:

		MLModel(int model_id, std::string model_description, std::string model_status_entry,
			std::string redis_state_entry, std::string model_path_entry, std::string log_file_path);
		virtual ~MLModel();
		
		/*******************************************************************************
		* GETTERS & SETTERS
		******************************************************************************/

		bool get_model_status(void) const;
		bool get_redis_state(void) const;

		/**
		* Loads the app configurations along with the torch script file located at (m_config_ptr[m_model_path_entry])
		*/
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
		void connect_to_redis(const std::vector<std::string>&& redis_entries = {});
		
		/**
		* Closes the existing Redis connection.
		*/
		void disconnect_from_redis(void);

		/**
		* Appends the provided data string to the specified redis list.
		*
		* \param redis_entry The identifier for the Redis list to which the provided string will be appended.
		* \param data_str The string to append.
		*/
		void write_str_to_redis(const std::string& redis_entry, std::string data_str);

		/*******************************************************************************
		* PURE VIRTUAL METHODS
		* When implemented, all of the following methods must be non-bloking
		******************************************************************************/

		/**
		* Launches the model evaluation in an other thread.
		* Must be non-blocking.
		*/
		virtual void start_stream(void) = 0;

		/**
		* Ends the model evaluation
		*/
		virtual void stop_stream(void) = 0;

	protected:

		void write_debug_output(const QString&);

		int m_model_id;
		bool m_model_status = false;
		bool m_stream_status = false;
		std::string m_model_description;

		// configs vars
		bool m_config_loaded = false;
		std::shared_ptr<config_map> m_config_ptr;
		std::string m_model_status_entry, m_model_path_entry;
		
		// redis vars
		bool m_redis_state = false;
		std::string m_redis_state_entry;
		std::unique_ptr<sw::redis::Redis> m_redis_client_p;

		std::ofstream m_log_file;
		torch::jit::script::Module m_model;

	signals:
		void debug_output(QString debug_str);
		void new_us_img_detection(QImage image);

};


/**
* Structure to encapsulates US image detection return data
*/
struct ImgDetectData {
	bool detected;
	float score;
	cv::Mat mask;
	cv::Rect bounding_box;
};


/**
* Class for the implementation of automatic ultrasound shell shape detection 
*/
class USImgDetector {

	public:

		USImgDetector() {}

		/*
		* \param template_path The path to the template for Ultrasound shell shapes (image file)
		*/
		USImgDetector(const std::string& template_path);
		
		/*
		* Attempts to detect a ultrasound shell shape contour inside of the provided image.
		* Part of the detection process use the template provided in the constructor.
		* 
		* \param img The image for which to perform the detection
		* \return The detection data (ImgDetectData)
		*/
		ImgDetectData detect(const cv::Mat& img);

	private:

		void find_large_contours(cv::Mat& img, std::vector<std::vector<cv::Point>>& contours);

		int m_min_contour_size = 500;
		float m_detection_tresh = 0.002;

		cv::Mat m_template_img;
		cv::Mat m_morph_kernel, m_horizontal_kernel;

};