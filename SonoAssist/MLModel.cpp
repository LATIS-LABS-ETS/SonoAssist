#include "MLModel.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// constructor && destructor

MLModel::MLModel(int model_id, const std::string& model_description, 
	const std::string& redis_state_entry, const std::string& model_path_entry, const std::string& log_file_path):
	m_model_id(model_id), m_model_description(model_description), m_redis_state_entry(redis_state_entry), m_model_path_entry(model_path_entry){

	if (log_file_path != "") {
		m_log_file.open(log_file_path, std::fstream::app);
	}

}

MLModel::~MLModel() {
	m_log_file.close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// getters and setters

bool MLModel::get_model_status(void) const {
	return m_model_status;
}

bool MLModel::get_redis_state(void) const {
	return m_redis_state;
}

void MLModel::set_configuration(std::shared_ptr<config_map> config_ptr) {

	m_config_ptr = config_ptr;

	// getting the redis status (is the device expected to connect to redis)
	try {
		m_redis_state = (*m_config_ptr)[m_redis_state_entry] == "true";
	} catch (...) {
		m_redis_state = false;
		write_debug_output("Failed to load redis status from config");
	}

	// loading the specified pytorch model
	try {
		m_model = torch::jit::load((*m_config_ptr)[m_model_path_entry]);
		m_model_status = true;
	} 
	catch (const c10::Error& e) {
		m_model_status = false;
		write_debug_output("Failed to load model from torch script file : " + QString::fromStdString(e.msg()));
	}
	catch (...) {
		m_model_status = false;
		write_debug_output("Failed to load model from torch script file");
	}

	m_config_loaded = true;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// redis methods

void MLModel::connect_to_redis(const std::vector<std::string>&& redis_entries) {

	try {

		// creating a new redis connection
		sw::redis::ConnectionOptions connection_options;
		connection_options.host = REDIS_ADDRESS;
		connection_options.port = REDIS_PORT;
		connection_options.socket_timeout = std::chrono::milliseconds(REDIS_TIMEOUT);
		m_redis_client_p = std::make_unique<sw::redis::Redis>(connection_options);

		// clearing the sepcified entries
		for (auto i = 0; i < redis_entries.size(); i++) {
			m_redis_client_p->del(redis_entries[i]);
		}

		m_redis_state = true;

	} catch (...) {
		m_redis_state = false;
		write_debug_output("Failed to connect to redis");
	}

}

void MLModel::disconnect_from_redis(void) {
	m_redis_client_p = nullptr;
}

void MLModel::write_str_to_redis(const std::string& redis_entry, std::string data_str) {

	if (m_redis_state && m_redis_client_p != nullptr) {

		try {
			m_redis_client_p->rpush(redis_entry, { data_str });
		} catch (...) {
			m_redis_state = false;
			write_debug_output("Failed to write (string) to redis");
		}

	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// helpers

/*
* Writes debug output to QDebug (the debug console) and the debug output window
*/
void MLModel::write_debug_output(const QString& debug_str) {

	QString out_str = QString::fromUtf8(m_model_description.c_str()) + " - " + debug_str;

	// sending msg to the application and vs debug windows
	qDebug() << "\n" + out_str;
	emit debug_output(out_str);

	// logging the message
	m_log_file << out_str.toStdString() << std::endl;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// US image detection

USImgDetector::USImgDetector(const std::string& template_path) {

	try {
		m_template_img = cv::imread(template_path, cv::IMREAD_GRAYSCALE);
	} catch (...) {}

	m_horizontal_kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(30, 1));
	m_morph_kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9), cv::Point(4, 4));
	
}

void USImgDetector::find_large_contours(cv::Mat& img, std::vector<std::vector<cv::Point>>& contours) {

	std::vector<cv::Vec4i> hierarchy;
	cv::findContours(img, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_NONE);

	for (auto it = contours.begin(); it != contours.end();) {
		
		if ((*it).size() < m_min_contour_size) {
			it = contours.erase(it);
		} else {
			it++;
		}
	
	}

}

ImgDetectData USImgDetector::detect(const cv::Mat& img) {

	ImgDetectData detection_data;
	detection_data.detected = false;

	if (img.cols != 0 && m_template_img.cols != 0) {

		cv::Mat filter_img = cv::Mat::zeros(img.size(), CV_8UC1);
		cv::Mat detection_img = cv::Mat::zeros(img.size(), CV_8UC1);

		// smoothing + tresholding
		cv::cvtColor(img, detection_img, CV_BGRA2GRAY);
		cv::bilateralFilter(detection_img, filter_img, 7, 75, 75);
		cv::adaptiveThreshold(filter_img, detection_img, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY_INV, 11, 2);

		// removing horizontal lines
		cv::Mat h_lines;
		std::vector<cv::Vec4i> h_hierarchy;
		std::vector<std::vector<cv::Point>> h_contours;
		cv::morphologyEx(detection_img, h_lines, cv::MORPH_OPEN, m_horizontal_kernel, cv::Point(-1, -1), 3);
		cv::findContours(h_lines, h_contours, h_hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
		cv::drawContours(detection_img, h_contours, -1, (0), cv::FILLED);

		// morphology
		cv::dilate(detection_img, detection_img, m_morph_kernel);
		cv::erode(detection_img, detection_img, m_morph_kernel);

		// detecting and going through large contours

		std::vector<std::vector<cv::Point>> contours, sub_contours;
		find_large_contours(detection_img, contours);

		for (auto& contour : contours) {

			// filling and eroding the current contour -> this will create subcontours
			cv::Mat contour_img = cv::Mat::zeros(img.size(), CV_8UC1);
			cv::drawContours(contour_img, std::vector<std::vector<cv::Point>>({ contour }), -1, (255), cv::FILLED);
			cv::erode(contour_img, contour_img, m_morph_kernel);

			// detecting and going through sub-contours
			find_large_contours(contour_img, sub_contours);
			for (auto& sub_contour : sub_contours) {

				// isolating the current sub contour
				cv::Mat sub_contour_img = cv::Mat::zeros(img.size(), CV_8UC1);
				cv::drawContours(sub_contour_img, std::vector<std::vector<cv::Point>>({ sub_contour }), -1, (255), cv::FILLED);

				// checking if the sub-contour matches the us template (detection)
				float moments_d = cv::matchShapes(m_template_img, sub_contour_img, cv::CONTOURS_MATCH_I2, 0);
				if (moments_d <= m_detection_tresh) {

					detection_data.detected = true;
					detection_data.score = moments_d;
					detection_data.bounding_box = cv::boundingRect(sub_contour);
					detection_data.mask = (sub_contour_img.clone())(detection_data.bounding_box);

					break;

				}

			}

		}

	}

	return detection_data;

}