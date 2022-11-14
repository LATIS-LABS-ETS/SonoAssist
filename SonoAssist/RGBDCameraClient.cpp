#include "RGBDCameraClient.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// GazeTracker public methods

RGBDCameraClient::~RGBDCameraClient() {
	disconnect_device();
}

void RGBDCameraClient::connect_device(void) {

	// making sure requirements are filled
	if (m_config_loaded && m_sensor_used) {
	
		write_debug_output("RGBDCameraClient - testing the connection to the camera");

		// testing connection with the camera
		try {			
			rs2::pipeline p;
			p.start();
			p.stop();
			m_device_connected = true;
			write_debug_output("RGBDCameraClient - successfully connected to the camera");
		} catch (...) {
			m_device_connected = false;
			write_debug_output("RGBDCameraClient - failed to connect to the camera");
		}

		emit device_status_change(m_device_id, m_device_connected);
	
	}

}

void RGBDCameraClient::disconnect_device(void) {

	m_device_connected = false;
	emit device_status_change(m_device_id, false);	

}

void RGBDCameraClient::start_stream() {

	// making sure requirements are filled
	if (m_device_connected && !m_device_streaming && m_output_file_loaded) {
	
		// setting the base recording configurations
		m_camera_cfg = rs2::config();
		m_camera_cfg.enable_stream(RS2_STREAM_COLOR, RGB_WIDTH, RGB_HEIGHT, RS2_FORMAT_BGR8, RGB_FPS);
		m_camera_cfg.enable_stream(RS2_STREAM_DEPTH, DEPTH_WIDTH, DEPTH_HEIGHT, RS2_FORMAT_Z16, DEPTH_FPS);

		// preview mode does not record the camera images
		if (!m_stream_preview && !m_pass_through) {
			m_camera_cfg.enable_record_to_file(m_output_file_str);
			m_output_index_file.open(m_output_index_file_str, std::fstream::app);
		} 

		// starting the acquisition pipeline
		m_camera_pipe = rs2::pipeline();
		m_camera_pipe.start(m_camera_cfg);
		
		// launching the image emitting / indexing
		// camera images are only sent to the UI in preview mode
		m_collect_data = true;
		m_collection_thread = std::thread(&RGBDCameraClient::collect_camera_data, this);

		m_device_streaming = true;
		
	}

}

void RGBDCameraClient::stop_stream() {

	// making sure requirements are filled
	if (m_device_connected && m_device_streaming) {
	
		// stoping the data image emitting thread, in preview mode
		m_collect_data = false;
		m_collection_thread.join();
		
		// stoping the acquisition pipeline
		m_camera_pipe.stop();

		// closing the output index
		if (!m_stream_preview) {
			m_output_index_file.close();
		}

		m_device_streaming = false;

	}

}

void RGBDCameraClient::set_output_file(const std::string& output_folder_path) {

	try {

		m_output_folder_path = output_folder_path;

		// defining output files
		m_output_file_str = output_folder_path + "/RGBD_camera_data.bag";
		m_output_index_file_str = output_folder_path + "/RGBD_camera_index.csv";
		if (m_output_index_file.is_open()) m_output_index_file.close();

		// defining the index file header
		m_output_index_file.open(m_output_index_file_str);
		m_output_index_file << "Time (us)" << std::endl;
		m_output_index_file.close();
		
		m_output_file_loaded = true;

	} catch (...) {
		write_debug_output("RGBDCameraClient - error occured while setting the output file");
	}
	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// collection function

/*
* Collects frames from the camera stream and makes them available to the main window.
* This function is meant to be executed in a seperate thread.
*/
void RGBDCameraClient::collect_camera_data(void) {

	void* frame_data_p = nullptr;

	// defining QImage for writting
	int resized_w = RGB_WIDTH / CAMERA_DISPLAY_RESIZE_FACTOR;
	int resized_h = RGB_HEIGHT / CAMERA_DISPLAY_RESIZE_FACTOR;
	QImage q_image(resized_w, resized_h, QImage::Format_RGB888);

	while (m_collect_data) {

		// grabbing the color image from the camera
		frame_data_p = (void*)m_camera_pipe.wait_for_frames().get_color_frame().get_data();

		// displaying images in preview mode
		if (m_stream_preview) {
		
			// converting captured frame to opencv Mat
			cv::Mat color_frame(cv::Size(RGB_WIDTH, RGB_HEIGHT), CV_8UC3, frame_data_p, cv::Mat::AUTO_STEP);

			// resizing the captured frame and binding to the Qimage
			cv::Mat resized_color_frame(resized_h, resized_w, CV_8UC3, q_image.bits(), q_image.bytesPerLine());
			cv::resize(color_frame, resized_color_frame, resized_color_frame.size(), 0, 0, cv::INTER_AREA);
			cv::cvtColor(resized_color_frame, resized_color_frame, CV_BGR2RGB);

			// emiting the frame and waiting 
			emit new_video_frame(q_image.copy());
			std::this_thread::sleep_for(std::chrono::milliseconds(CAMERA_DISPLAY_THREAD_DELAY_MS));

		}

		// main mode
		else {

			// indexing received images in main mode
			if (!m_pass_through) {
				m_output_index_file << get_micro_timestamp() << "\n";
			}

		}
		
	}

}