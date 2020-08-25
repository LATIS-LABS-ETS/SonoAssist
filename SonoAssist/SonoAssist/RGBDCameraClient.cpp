#include "RGBDCameraClient.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// GazeTracker public methods

RGBDCameraClient::~RGBDCameraClient() {
	disconnect_device();
}

void RGBDCameraClient::connect_device(void) {

	// making sure requirements are filled
	if (m_config_loaded && m_output_file_loaded && m_sensor_used) {
	
		// testing connection with the camera
		try {
			rs2::pipeline p;
			p.start();
			p.stop();
			m_device_connected = true;
		} catch (...) {
			m_device_connected = false;
		}

		emit device_status_change(m_device_connected);
	
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
	
		// definiing recording configurations
		m_camera_cfg_p = std::make_unique<rs2::config>();
		m_camera_cfg_p->enable_record_to_file(m_camera_output_file_str);
		m_camera_cfg_p->enable_stream(RS2_STREAM_COLOR, RGB_WIDTH, RGB_HEIGHT, RS2_FORMAT_BGR8, RGB_FPS);
		m_camera_cfg_p->enable_stream(RS2_STREAM_DEPTH, DEPTH_WIDTH, DEPTH_HEIGHT, RS2_FORMAT_Z16, DEPTH_FPS);

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

/*
* Collects frames from the camera stream and makes them available to the main window.
* This function is meant to be executed in a seperate thread.
*/
void RGBDCameraClient::collect_camera_data(void) {

	// defining QImage for writting
	void* frame_data_p = nullptr;
	int resized_w = RGB_WIDTH / DISPLAY_RESIZE_FACTOR;
	int resized_h = RGB_HEIGHT / DISPLAY_RESIZE_FACTOR;
	QImage q_image(resized_w, resized_h, QImage::Format_RGB888);

	while (m_collect_data) {

		// collecting the color frame data
		frame_data_p = (void*) m_camera_pipe_p->wait_for_frames().get_color_frame().get_data();

		// converting captured frame to opencv Mat
		cv::Mat color_frame(cv::Size(RGB_WIDTH, RGB_HEIGHT), CV_8UC3, frame_data_p, cv::Mat::AUTO_STEP);

		// resizing the captured frame and binding to the Qimage
		cv::Mat resized_color_frame(resized_h, resized_w, CV_8UC3, q_image.bits(), q_image.bytesPerLine());
		cv::resize(color_frame, resized_color_frame, resized_color_frame.size(), 0, 0, cv::INTER_AREA);
		cv::cvtColor(resized_color_frame, resized_color_frame, CV_BGR2RGB);

		// emiting the frame and waiting
		emit new_video_frame(std::move(q_image.copy()));
		std::this_thread::sleep_for(std::chrono::milliseconds(DISPLAY_THREAD_DELAY_MS));

	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// setters and getters

void RGBDCameraClient::set_output_file(std::string output_folder_path) {

	try {
		// defining the output file path
		m_camera_output_file_str = output_folder_path + "/camera_data.bag";
		m_output_file_loaded = true;

	} catch (...) {
		qDebug() << "n\RGBDCameraClient - error occured while setting the output file";
	}

}