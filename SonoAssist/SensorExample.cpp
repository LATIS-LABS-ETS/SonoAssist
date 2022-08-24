#include "SensorExample.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// SensorExample public methods

void SensorExample::connect_device() {

    if (m_config_loaded && m_sensor_used) {

       // connect to the sensor
       // ...
       
       // once the connection is confirmed, changing the sensor's connection state 
       m_device_connected = true;
    }

    // reporting the connection state to the UI
    emit device_status_change(m_device_id, m_device_connected);

}

void SensorExample::disconnect_device() {

    // disconnect from the sensor
    // ...

    // once the disconnection is confirmed, changing the sensor's connection state 
    m_device_connected = false;

    // reporting the connection state to the UI
    emit device_status_change(m_device_id, false);

}

void SensorExample::start_stream() {

    if (m_device_connected && !m_device_streaming && m_output_file_loaded) {

        // opening the output file in append mode
        m_output_data_file.open(m_output_data_file_str, std::fstream::app);

        // connecting to redis (if redis is enabled) - specifying the proper config fields
        if (m_redis_state) {
            m_redis_entry = (*m_config_ptr)["example_redis_entry"];
            m_redis_rate_div = std::atoi((*m_config_ptr)["example_redis_rate_div"].c_str());
            connect_to_redis({""});
        }

        // init random for demonstration purposes
        srand(time(NULL));

        // launching the acquisition thread
        m_collect_data = true;
        m_collection_thread = std::thread(&SensorExample::collect_sensor_data, this);

        // once the acquisition process is launched, changing the sensor's acquisition state 
        m_device_streaming = true;

    }

}

void SensorExample::stop_stream() {

    if (m_device_streaming) {

        // stopping the data collection thread
        m_collect_data = false;
        m_collection_thread.join();

        // once the acquisition process is stoped, changing the sensor's acquisition state 
        m_device_streaming = false;

        // closing output files + redis
        m_output_data_file.close();
        disconnect_from_redis();

    }

}

void SensorExample::set_output_file(std::string output_folder_path) {

    try {

        m_output_folder_path = output_folder_path;

        // defining the output file
        m_output_data_file_str = output_folder_path + "/example_data.csv";
        if (m_output_data_file.is_open()) m_output_data_file.close();

        // defining the outputfile header (first row of the csv)
        m_output_data_file.open(m_output_data_file_str);
        m_output_data_file << "Time (us), Data" << std::endl;
        m_output_data_file.close();

        // once the outputs files are created, changing the sensor's output file state 
        m_output_file_loaded = true;

    }
    catch (...) {
        write_debug_output("SensorExample - error occured while setting the output file");
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// data collection function

/*
* Collects the most recent screen capture and makes it available to the main window.
* This function is meant to be executed in a seperate thread.
*/
void SensorExample::collect_sensor_data(void) {

    while (m_collect_data) {

        // not writing data to output file for preview mode and pass through mode
        if (!m_stream_preview && !m_pass_through) {

            // collect data from the sensor
            // ...

            std::string collected_data = std::to_string(rand() % 10 + 1);
            
            // writing to the output file
           m_output_data_file << get_micro_timestamp() << "," << collected_data << "\n";
           
            // writing to redis
           write_str_to_redis(m_redis_state_entry, collected_data);

        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    }

}