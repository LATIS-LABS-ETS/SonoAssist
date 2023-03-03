#include "GazeTracker.h"

/*******************************************************************************
* HELPER & CALLBACK FUNCTIONS
******************************************************************************/

void url_receiver(char const* url, void* user_data){
	
	char* buffer = (char*) user_data;
	// only keep first value
	if (*buffer != '\0') return; 

	if (strlen(url) < 256)
		strcpy(buffer, url);
}

void gaze_point_callback(tobii_gaze_point_t const* gaze_point, void* user_data) {
	
	int64_t tobii_time;
	GazeTracker* manager = (GazeTracker*)user_data;

	if (gaze_point->validity == TOBII_VALIDITY_VALID) {

		// preview mode just emits gaze points to the UI
		if (manager->get_stream_preview_status()) {
			emit manager->new_gaze_point(gaze_point->position_xy[0], gaze_point->position_xy[1]);
		}

		// only writting out data in main display mode
		else {
		
			// getting timestamp strings
			tobii_system_clock(manager->m_tobii_api, &tobii_time);
			std::string reception_time_os = manager->get_micro_timestamp();
			std::string reception_time_tobii = std::to_string(tobii_time);
			std::string data_time_tobii = std::to_string(gaze_point->timestamp_us);

			// defining the output string
			std::string output_str = reception_time_os + "," + reception_time_tobii + "," + data_time_tobii + "," +
				std::to_string(gaze_point->position_xy[0]) + "," + std::to_string(gaze_point->position_xy[1]) + "\n";

			// writing to redis
			manager->write_str_to_redis(manager->m_redis_entry, output_str);

			// writing to output files, after passthrough check
			if (!manager->get_pass_through()) {
				manager->m_output_gaze_file << output_str;
			}
			
		}

	}

}

void head_pose_callback(tobii_head_pose_t const* head_pose, void* user_data) {

	int64_t tobii_time;
	GazeTracker* manager = (GazeTracker*)user_data;

	if (head_pose->position_validity == TOBII_VALIDITY_VALID) {
	
		// only writting out data in main display mode
		if (!manager->get_stream_preview_status() && !manager->get_pass_through()) {
		
			// getting timestamp strings
			tobii_system_clock(manager->m_tobii_api, &tobii_time);
			std::string reception_time_os = manager->get_micro_timestamp();
			std::string reception_time_tobii = std::to_string(tobii_time);
			std::string data_time_tobii = std::to_string(head_pose->timestamp_us);
			
			// defining the output string
			std::string output_str = reception_time_os + "," + reception_time_tobii + "," + data_time_tobii + "," +
				std::to_string(head_pose->position_xyz[0]) + "," + std::to_string(head_pose->position_xyz[1]) + "," + 
				std::to_string(head_pose->position_xyz[2]) + "\n";

			manager->m_output_head_file << output_str;

		}

	}

}

/*******************************************************************************
* CONSTRUCTOR & DESTRUCTOR
******************************************************************************/

GazeTracker::GazeTracker(int device_id, const std::string& device_description, 
	const std::string& redis_state_entry, const std::string& log_file_path): 
	SensorDevice(device_id, device_description, redis_state_entry, log_file_path)
{

	// creating tobii api object
	tobii_error_t error = tobii_api_create(&m_tobii_api, NULL, NULL);
	m_tobii_api_valid = (error == TOBII_ERROR_NO_ERROR);	

}

GazeTracker::~GazeTracker() {

	disconnect_device();
	tobii_api_destroy(m_tobii_api);

}

/*******************************************************************************
* SENSOR DEVICE OVERRIDES
******************************************************************************/

void GazeTracker::connect_device(void) {

	tobii_error_t error;
	char url[256] = { 0 };

	if (m_tobii_api_valid && m_config_loaded && m_sensor_used) {

		// making sure device is disconnected
		tobii_head_pose_unsubscribe(m_tobii_device);
		tobii_gaze_point_unsubscribe(m_tobii_device);
		tobii_device_destroy(m_tobii_device);

		// initializing communication (device level)
		try {
			
			error = tobii_enumerate_local_device_urls(m_tobii_api, url_receiver, url);
			error = tobii_device_create(m_tobii_api, url, TOBII_FIELD_OF_USE_INTERACTIVE, &m_tobii_device);
			
			// subscribing the gaze data callback
			// subscribing the head pose data callback
			error = tobii_gaze_point_subscribe(m_tobii_device, gaze_point_callback, (void*)this);
			error = tobii_head_pose_subscribe(m_tobii_device, head_pose_callback, (void*)this);
			
			if (error == TOBII_ERROR_NO_ERROR) m_device_connected = true;
		
		} catch (...) { }
		
		emit device_status_change(m_device_id, m_device_connected);

	}
}

void GazeTracker::disconnect_device(void) {
	
	// unsubscribing the callback and destroying device
	if (m_tobii_device != nullptr) {
		tobii_head_pose_unsubscribe(m_tobii_device);
		tobii_gaze_point_unsubscribe(m_tobii_device);
		tobii_device_destroy(m_tobii_device);
		m_tobii_device = nullptr;
	}

	m_device_connected = false;
	emit device_status_change(m_device_id, false);

}

void GazeTracker::start_stream() {

	if (m_device_connected && !m_device_streaming && m_output_file_loaded) {
	
		// opening the output files
		set_output_file(m_output_folder_path);
		m_output_head_file.open(m_output_head_str, std::fstream::app);
		m_output_gaze_file.open(m_output_gaze_str, std::fstream::app);

		// connecting to redis (if redis enabled)
		if (m_redis_state) {
			m_redis_entry = (*m_config_ptr)["eye_tracker_redis_entry"];
			m_redis_rate_div = std::atoi((*m_config_ptr)["eye_tracker_redis_rate_div"].c_str());
			connect_to_redis({m_redis_entry});
		}

		// launching the collection thread
		m_collect_data = true;
		m_collection_thread = std::thread(&GazeTracker::collect_data, this);
		m_device_streaming = true;
	
	}

}

void GazeTracker::stop_stream() {
	
	if (m_device_connected && m_device_streaming) {
	
		// stop the streaming thread
		m_collect_data = false;
		m_collection_thread.join();

		// closing the output file redis connection
		m_output_gaze_file.close();
		m_output_head_file.close();
		disconnect_from_redis();
	
		m_device_streaming = false;

	}

}

void GazeTracker::set_output_file(const std::string& output_folder_path) {

	try {

		m_output_folder_path = output_folder_path;

		// defining the output files
		m_output_head_str = output_folder_path + "/eye_tracker_head.csv";
		m_output_gaze_str = output_folder_path + "/eye_tracker_gaze.csv";
		if (m_output_head_file.is_open()) m_output_head_file.close();
		if (m_output_gaze_file.is_open()) m_output_gaze_file.close();

		// writting the head pose file header
		m_output_head_file.open(m_output_head_str);
		m_output_head_file << "Reception OS time,Reception tobii time,Onboard time,X,Y,Z" << std::endl;
		m_output_head_file.close();

		// writting the gaze file header
		m_output_gaze_file.open(m_output_gaze_str);
		m_output_gaze_file << "Reception OS time,Reception tobii time,Onboard time,X,Y" << std::endl;
		m_output_gaze_file.close();

		m_output_file_loaded = true;

	} catch (...) {
		write_debug_output("\nGazeTracker - error occured while setting the output file");
	}

}

/*******************************************************************************
* DATA COLLECTION FUNCTIONS
******************************************************************************/

void GazeTracker::collect_data(void) {

	// continuously wait for data and call callbacks
	while (m_collect_data) {
		tobii_wait_for_callbacks(1, &m_tobii_device);
		tobii_device_process_callbacks(m_tobii_device);
	}

}