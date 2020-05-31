#include "GazeTracker.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// helper / callback functions

void url_receiver(char const* url, void* user_data){
	
	char* buffer = (char*) user_data;
	// only keep first value
	if (*buffer != '\0') return; 

	if (strlen(url) < 256)
		strcpy(buffer, url);
}

void gaze_data_callback(tobii_gaze_point_t const* gaze_point, void* user_data) {
	
	// making sure data is valid
	if (gaze_point->validity == TOBII_VALIDITY_VALID) {

		// getting the eye tracker manager
		GazeTracker* manager = (GazeTracker*)user_data;

		// generating millisecond time stamp and output string
		auto time_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		std::string output_str = std::to_string(time_stamp) + "," + std::to_string(gaze_point->position_xy[0]) + ","
			+ std::to_string(gaze_point->position_xy[1]) + "\n";

		// writing to the output file
		manager->m_output_file << output_str;
		
		// writing to redis
		if (manager->m_redis_client.is_connected()) {
			if ((manager->m_redis_data_count % manager->m_redis_rate_div) == 0) {
				manager->m_redis_client.rpushx(manager->m_redis_entry, output_str);
				manager->m_redis_client.sync_commit();
				manager->m_redis_data_count = 1;
			}
			else {
				manager->m_redis_data_count++;
			}
		}

	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// GazeTracker public methods

GazeTracker::GazeTracker() {
	// creating tobii api object
	tobii_error_t error = tobii_api_create(&m_tobii_api, NULL, NULL);
	m_tobii_api_valid = (error == TOBII_ERROR_NO_ERROR);	
}

GazeTracker::~GazeTracker() {
	disconnect_device();
	tobii_api_destroy(m_tobii_api);
}

void GazeTracker::connect_device(void) {

	tobii_error_t error;
	char url[256] = { 0 };

	// making sure requirements are filled
	if (m_tobii_api_valid && m_config_loaded && m_output_file_loaded) {

		// making sure device is disconnected
		disconnect_device();

		// initializing communication (device level)
		error = tobii_enumerate_local_device_urls(m_tobii_api, url_receiver, url);
		if (error != TOBII_ERROR_NO_ERROR && *url != '\0') return;
		error = tobii_device_create(m_tobii_api, url, TOBII_FIELD_OF_USE_INTERACTIVE, &m_tobii_device);
		if (error != TOBII_ERROR_NO_ERROR) return;

		// subscribing the gaze data callback
		error = tobii_gaze_point_subscribe(m_tobii_device, gaze_data_callback, (void*) this);
		if (error != TOBII_ERROR_NO_ERROR) return;

		m_device_connected = true;
		emit device_status_change(true);

	}
}

void GazeTracker::disconnect_device(void) {
	
	// making sure requirements are filled
	if (m_device_connected) {
		
		// unsubscribing the callback and destroying device
		tobii_gaze_point_unsubscribe(m_tobii_device);
		tobii_device_destroy(m_tobii_device);
		
		m_device_connected = false;
		emit device_status_change(false);

	}

}

void GazeTracker::start_stream() {

	// making sure requirements are filled
	if (m_device_connected && !m_device_streaming) {
	
		// opening the output file
		m_output_file.open(m_output_file_str, std::fstream::app);

		// connecting to redis (redis enabled)
		if ((*m_config_ptr)["eye_tracker_to_redis"] == "true") {

			// loading redis parameters
			m_redis_entry = (*m_config_ptr)["eye_tracker_redis_entry"];
			m_redis_rate_div = std::atoi((*m_config_ptr)["eye_tracker_redis_rate_div"].c_str());

			// connecting to redis and initializing the data list
			m_redis_client.connect();
			m_redis_client.del(std::vector<std::string>({ m_redis_entry }));
			m_redis_client.rpush(m_redis_entry, std::vector<std::string>({ "" }));
			m_redis_client.sync_commit();

		}

		// launching the collection thread
		m_collect_data = true;
		m_device_streaming = true;
		m_collection_thread = std::thread(&GazeTracker::collect_gaze_data, this);
	
	}

}

void GazeTracker::stop_stream() {
	
	// making sure requirements are filled
	if (m_device_connected && m_device_streaming) {
	
		// stop the streaimg thread
		m_collect_data = false;
		m_collection_thread.join();

		// closing the output file redis connection
		m_output_file.close();
		if ((*m_config_ptr)["eye_tracker_to_redis"] == "true") {
			m_redis_client.disconnect();
		}

		m_device_streaming = false;

	}

}

void GazeTracker::collect_gaze_data(void) {

	// continuously wait for data and call callbacks
	while (m_collect_data) {
		tobii_wait_for_callbacks(1, &m_tobii_device);
		tobii_device_process_callbacks(m_tobii_device);
	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// setters and getters

void GazeTracker::set_output_file(std::string output_file_path, std::string extension) {

	try {

		// defining the output file path
		auto extension_pos = output_file_path.find(extension);
		m_output_file_str = output_file_path.replace(extension_pos, extension.length(), "_eye_tracker.csv");
		if (m_output_file.is_open()) m_output_file.close();

		// writing the output file header
		m_output_file.open(m_output_file_str);
		m_output_file << "Time" << "," << "X coordinate" << "," << "Y coordinate" << std::endl;
		m_output_file.close();
		m_output_file_loaded = true;

	} catch (...) {
		qDebug() << "n\GazeTracker - error occured while setting the output file";
	}

}