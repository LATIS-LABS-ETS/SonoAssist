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
* Interface for the implementation ML models loaded from torch scripts
*/
class MLModel : public QObject {

	Q_OBJECT

	public:

		MLModel(int model_id, std::string model_description, std::string model_status_entry,
			std::string redis_state_entry, std::string model_path_entry, std::string log_file_path);
		virtual ~MLModel();

		// interface methods (virtual)
		// all interface functions must be non-bloking
		virtual void eval(void) = 0;
		virtual void start_stream(void) = 0;
		virtual void stop_stream(void) = 0;
		
		// setters and getters
		bool get_model_status(void) const;
		bool get_redis_state(void) const;
		void set_configuration(std::shared_ptr<config_map> config_ptr);

		// redis methods
		void connect_to_redis(const std::vector<std::string> && = {});
		void disconnect_from_redis(void);
		void write_str_to_redis(const std::string&, std::string);

	protected:
		void write_debug_output(const QString&);

	protected:

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


/*Encapsulates US image detection return data*/
struct ImgDetectData {
	bool detected;
	float score;
	cv::Mat mask;
	cv::Rect bounding_box;
};


/* Implements automatic US shell shape detection */
class USImgDetector {

	public:
		USImgDetector(){};
		USImgDetector(const std::string& template_path);
		ImgDetectData detect(const cv::Mat& img);

	private:
		void find_large_contours(cv::Mat& img, std::vector<std::vector<cv::Point>>& contours);

	private:

		int m_min_contour_size = 500;
		float m_detection_tresh = 0.002;

		cv::Mat m_template_img;
		cv::Mat m_morph_kernel, m_horizontal_kernel;

};