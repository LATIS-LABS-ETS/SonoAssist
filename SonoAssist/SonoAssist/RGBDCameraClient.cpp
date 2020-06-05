#include "RGBDCameraClient.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// GazeTracker public methods

RGBDCameraClient::~RGBDCameraClient() {
	disconnect_device();
}


void RGBDCameraClient::connect_device(void) {

	// making sure requirements are filled
	if (m_config_loaded && m_output_file_loaded) {

		// making sure device is disconnected
		disconnect_device();

		// enabling color and depth video streams
		m_camera_cfg_p = std::make_unique<rs2::config>();
		m_camera_cfg_p->enable_record_to_file(m_camera_output_file_str);
		m_camera_cfg_p->enable_stream(RS2_STREAM_COLOR, RGB_WIDTH, RGB_HEIGTH, RS2_FORMAT_BGR8, RGB_FPS);
		m_camera_cfg_p->enable_stream(RS2_STREAM_DEPTH, DEPTH_WIDTH, DEPTH_HEIGTH, RS2_FORMAT_Z16, DEPTH_FPS);

		// changing device state
		m_device_connected = true;
		emit device_status_change(true);
		
	}

}

void RGBDCameraClient::disconnect_device(void) {

	// making sure requirements are filled
	if (m_device_connected) {
		m_device_connected = false;
		emit device_status_change(false);
	}

}

void RGBDCameraClient::start_stream() {

	// making sure requirements are filled
	if (m_device_connected && !m_device_streaming) {
	
		// starting pipeline and initializing video writters
		m_camera_pipe_p = std::make_unique<rs2::pipeline>();
		m_camera_pipe_p->start(*m_camera_cfg_p);
		
		// launching the acquisition thread
		m_collect_data = true;
		m_collection_thread = std::thread(&RGBDCameraClient::collect_camera_data, this);
		m_device_streaming = true;
	}

}

void RGBDCameraClient::stop_stream() {

	// making sure requirements are filled
	if (m_device_connected && m_device_streaming) {
	
		// stoping the data collection thread
		m_collect_data = false;
		m_collection_thread.join();

		// stoping the pipeline
		m_camera_pipe_p->stop();
		m_camera_pipe_p.reset();
		m_camera_cfg_p.reset();

		m_device_streaming = false;

	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// collection function

void RGBDCameraClient::collect_camera_data(void) {

	rs2::frameset frames;

	// dropping first frames (camera warmup)
	for (auto i = 0; i < N_SKIP_FRAMES; i++) 
		frames = m_camera_pipe_p->wait_for_frames();

	while (m_collect_data) {
	
		// collecting frames from the enabled strames
		frames = m_camera_pipe_p->wait_for_frames();

		// converting the color frame to a MAT and making available
		m_latest_frame = cv::Mat(cv::Size(RGB_WIDTH, RGB_HEIGTH), CV_8UC3,
			(void*)frames.get_color_frame().get_data(), cv::Mat::AUTO_STEP);
	}

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// setters and getters

cv::Mat RGBDCameraClient::get_latest_frame() {
	return m_latest_frame;
}

void RGBDCameraClient::set_output_file(std::string output_file_path, std::string extension) {

	try {
		// defining the output file path
		auto extension_pos = output_file_path.find(extension);
		m_camera_output_file_str = output_file_path.replace(extension_pos, extension.length(), "_camera_data.bag");
		m_output_file_loaded = true;

	} catch (...) {
		qDebug() << "n\RGBDCameraClient - error occured while setting the output file";
	}

}