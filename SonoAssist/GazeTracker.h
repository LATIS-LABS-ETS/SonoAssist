#ifndef GAZETRACKER_H
#define GAZETRACKER_H

#include "SensorDevice.h"

#include <string>
#include <thread>
#include <fstream>
#include <exception>

#include <tobii/tobii.h>
#include <tobii/tobii_streams.h>

/*
* Class to enable communication with the tobii eye tracker 4C
*/
class GazeTracker : public SensorDevice {

	Q_OBJECT

	public:

		GazeTracker(int device_id, std::string device_description, std::string redis_state_entry, std::string log_file_path);
		~GazeTracker();

		// SensorDevice interface functions
		void stop_stream(void);
		void start_stream(void);
		void connect_device(void);
		void disconnect_device(void);
		void set_output_file(std::string output_folder);

		// threaded collection method and callback
		void collect_data(void);

		// tobii communication vars (accessed from callbacks)
		bool m_tobii_api_valid = false;
		tobii_api_t* m_tobii_api = nullptr;
		tobii_device_t* m_tobii_device = nullptr;

		// output file attributes + redis
		std::string m_redis_entry = "";
		std::ofstream m_output_gaze_file;
		std::ofstream m_output_head_file;


	signals:
		void new_gaze_point(float x , float y);

	private:

		// output file vars
		bool m_output_file_loaded = false;
		std::string m_output_gaze_str = "";
		std::string m_output_head_str = "";

		// streaming vars
		bool m_collect_data = false;
		std::thread m_collection_thread;

};

// helper and call back function prototypes
void url_receiver(char const* url, void* user_data);
void head_pose_callback(tobii_head_pose_t const* head_pose, void* user_data);
void gaze_point_callback(tobii_gaze_point_t const* gaze_point, void* user_data);

#endif