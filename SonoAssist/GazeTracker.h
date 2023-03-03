#pragma once

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

		GazeTracker(int device_id, const std::string& device_description,
			const std::string& redis_state_entry, const std::string& log_file_path);
		~GazeTracker();

		void stop_stream(void) override;
		void start_stream(void) override;
		void connect_device(void) override;
		void disconnect_device(void) override;
		void set_output_file(const std::string& output_folder) override;
		
		// tobii communication vars (accessed from callbacks)
		bool m_tobii_api_valid = false;
		tobii_api_t* m_tobii_api = nullptr;
		tobii_device_t* m_tobii_device = nullptr;

		// output file attributes + redis
		std::string m_redis_entry = "";
		std::ofstream m_output_gaze_file;
		std::ofstream m_output_head_file;

	private:

		/**
		* Eye tracker data collection function
		* This function is meant to be ran in a seperate thread. It waits for data to be available to the callbacks then
		* triggers a call to the appropriate one via the "tobii_device_process_callbacks" function.
		*/
		void collect_data(void);

		// output file vars
		bool m_output_file_loaded = false;
		std::string m_output_gaze_str = "";
		std::string m_output_head_str = "";

		// streaming vars
		bool m_collect_data = false;
		std::thread m_collection_thread;

	signals:
		void new_gaze_point(float x, float y);
};


/*******************************************************************************
* HELPER & CALLBACK FUNCTIONS
******************************************************************************/

/**
* Copies the url in to the user data
*/
void url_receiver(char const* url, void* user_data);

/**
* Callback function for the collection of head position data (x, y, z coordinates (mm) from the center of the screen)
* This function is called by the collection thread every time a new measure is available
*
* \param gaze_point structure containing the head pose data
* \param user_data voided context variable which was passed to the API upon callback registration.
*		 Pointer to the GazeTracker object interfacing with the eyetracker.
*/
void head_pose_callback(tobii_head_pose_t const* head_pose, void* user_data);

/**
* Callback function for the collection of gaze point data ( relative (x, y) coordinates on the screen)
* This function is called by the collection thread every time a new gaze point is available
*
* \param gaze_point structure containing the gaze point data
* \param user_data voided context variable which was passed to the API upon callback registration. Pointer
*		 to the GazeTracker object interfacing with the eyetracker.
*/
void gaze_point_callback(tobii_gaze_point_t const* gaze_point, void* user_data);