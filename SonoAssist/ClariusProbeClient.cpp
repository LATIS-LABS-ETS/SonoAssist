#include "ClariusProbeClient.h"

// global pointer for the Clarius callback(s)
static ClariusProbeClient* probe_client_p = nullptr;

/*******************************************************************************
* ACQUISITION CALLBACK
******************************************************************************/

/**
* Callback function for incoming processed images (as displayed on the tablet) from the Clarius probe
* Writes acquired data (IMU and images) to output files and emits the US images to the main window
*/
 void new_processed_image_callback(const void* img, const ClariusProcessedImageInfo* nfo, int npos, const ClariusPosInfo* pos) {
       
    // dropping the current frame if display is buisy
     if (!probe_client_p->m_handler_locked) {
     
         probe_client_p->m_handler_locked = true;

         // taking note of the reception time
         probe_client_p->m_reception_time = probe_client_p->get_micro_timestamp();

         // notification message for missing IMU data
         if ((npos < 1) && !probe_client_p->m_imu_missing) {
             probe_client_p->m_imu_missing = true;
             emit probe_client_p->no_imu_data("Check clarius probe settings", "No incoming IMU data from the clarius probe.");
         }
        
         // mapping the incoming image to a Mat (no copy) + gray scale conversion
         probe_client_p->m_input_img_mat.data = static_cast<uchar*>(const_cast<void*>(img));
         cv::cvtColor(probe_client_p->m_input_img_mat, probe_client_p->m_cvt_mat, CV_BGRA2GRAY);
         cv::resize(probe_client_p->m_cvt_mat, probe_client_p->m_output_img_mat,
             probe_client_p->m_output_img_mat.size(), 0, 0, cv::INTER_AREA);

         // defining the output data destined for the csv file
         if (probe_client_p->get_stream_status() && !probe_client_p->get_stream_preview_status()) {

             probe_client_p->m_onboard_time = std::to_string(nfo->tm);
             
             std::string imu_entry;
             for (int i = 0; i < npos; i++) {
                 imu_entry = std::to_string(pos[i].gx) + "," + std::to_string(pos[i].gy) + "," + std::to_string(pos[i].gz) + "," +
                             std::to_string(pos[i].ax) + "," + std::to_string(pos[i].ay) + "," + std::to_string(pos[i].az) + "," + 
                             std::to_string(pos[i].mx) + "," + std::to_string(pos[i].my) + "," + std::to_string(pos[i].mz) + "," + 
                             std::to_string(pos[i].qw) + "," + std::to_string(pos[i].qx) + "," + std::to_string(pos[i].qy) + "," + std::to_string(pos[i].qz);
                 probe_client_p->m_imu_data.push_back(imu_entry);
             }

         }

         // passing the image to the diplay (pass by copy because the image is needed elsewhere)
         probe_client_p->m_display_locked = false;
         if (probe_client_p->get_stream_preview_status()) {
             emit probe_client_p->new_us_preview_image(probe_client_p->m_output_img);
         } else {
             emit probe_client_p->new_us_image(probe_client_p->m_output_img);
         }

     }

}

 /*******************************************************************************
 * SENSOR DEVICE OVERRIDES
 ******************************************************************************/

void ClariusProbeClient::connect_device() {

    if (m_config_loaded && m_sensor_used) {

        clariusDestroyListener();

        // global instance pointer (for the clarius callbacks)
        if (probe_client_p == nullptr) probe_client_p = this;

        // setting up the clarius listener + registering the new probe image callback
        if (clariusInitListener(0, nullptr, 
                QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString().c_str(),
                new_processed_image_callback, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                CLARIUS_NORMAL_DEFAULT_WIDTH, CLARIUS_NORMAL_DEFAULT_HEIGHT) == 0){
            
            m_device_connected = true;
            write_debug_output("ClariusProbeClient - successfully connected\n");
        
        } else {
            write_debug_output("ClariusProbeClient - failed to connect\n");
        }

    }

    emit device_status_change(m_device_id, m_device_connected);

}

void ClariusProbeClient::disconnect_device() {

    if (clariusDestroyListener() == 0) {
        write_debug_output("ClariusProbeClient - destroyed the listener\n");
    } else {
        write_debug_output("ClariusProbeClient - failed to destroy the listener\n");
    }
                
    m_device_connected = false;
    emit device_status_change(m_device_id, false);
   
}

void ClariusProbeClient::start_stream() {
    
    if (m_device_connected && !m_device_streaming && m_output_file_loaded) {
        
        // preparing the US image callback
        m_imu_missing = false;
        m_display_locked = true;
        m_handler_locked = false;

        // preparing for image handling
        configure_img_acquisition();
        initialize_img_handling();

        // preparing the writing of data
        set_output_file(m_output_folder_path);
        m_output_imu_file.open(m_output_imu_file_str, std::fstream::app);
        m_video = cv::VideoWriter(m_output_video_file_str, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
            CLARIUS_VIDEO_FPS, cv::Size(m_out_img_width, m_out_img_height), true);

        // connecting to redis (if redis enabled)
        if (m_redis_state) {
            m_redis_imu_entry = (*m_config_ptr)["us_probe_imu_redis_entry"];
            m_redis_img_entry = (*m_config_ptr)["us_probe_img_redis_entry"];
            m_redis_rate_div = std::atoi((*m_config_ptr)["us_probe_redis_rate_div"].c_str());
            connect_to_redis({m_redis_imu_entry, m_redis_img_entry});
        }

        // connecting to the probe events
        try {
            if (!(clariusConnect((*m_config_ptr)["us_probe_ip_address"].c_str(), m_udp_port, nullptr) < 0)){    
                m_device_streaming = true;
            }  
        } catch (...) {}

        if (m_device_streaming) write_debug_output("ClariusProbeClient - successfully started the acquisition\n");
        else write_debug_output("ClariusProbeClient - failed to start the acquisition\n");

    }

}

void ClariusProbeClient::stop_stream() {
    
    if (m_device_streaming) {

        m_device_streaming = false;

        // stopping the acquisition
        if (clariusDisconnect(nullptr) == 0) {
            write_debug_output("ClariusProbeClient - disconnected\n");
        } else {
            write_debug_output("ClariusProbeClient - failed to disconnect\n");
        }

        // closing the outputs
        while(m_writing_ouput);
        m_video.release();
        m_output_imu_file.close();
        disconnect_from_redis();

    }

}

void ClariusProbeClient::set_output_file(const std::string& output_folder_path) {

    try {

        m_output_folder_path = output_folder_path;

        // defining the output file path
        m_output_imu_file_str = output_folder_path + "/clarius_data.csv";
        m_output_video_file_str = output_folder_path + "/clarius_images.avi";
        if (m_output_imu_file.is_open()) m_output_imu_file.close();

        // writing the output file header
        m_output_imu_file.open(m_output_imu_file_str);
        m_output_imu_file << "Reception OS time,Display OS time,Onboard time,gx,gy,gz,ax,ay,az,mx,my,mz,qw,qx,qy,qz" << std::endl;
        m_output_imu_file.close();

        m_output_file_loaded = true;

    } catch (...) {
        write_debug_output("ClariusProbeClient - error occured while setting the output file");
    }

}

/*******************************************************************************
* UTILITY & HELPER METHODS
******************************************************************************/

void ClariusProbeClient::set_udp_port(int port) {
    m_udp_port = port;
}

void ClariusProbeClient::write_output_data() {

    try {

        // building the imu data string (including the 3 different timestamps)
        std::string imu_str = m_reception_time + "," + m_display_time + "," + m_onboard_time + ",";
        if (m_imu_data.size() == 0) {
            imu_str += " , , , , , , , , , , , , \n";
        } else {
            imu_str += m_imu_data[0] + "\n";
            for (int i = 1; i < m_imu_data.size(); i++)
                imu_str += " , , ," + m_imu_data[i] + "\n";
        }

        m_writing_ouput = true;

        // converting the display format to the video format
        cv::cvtColor(m_output_img_mat, m_video_img_mat, CV_GRAY2BGR);

        // writing the probe data to redis (output image is grayscale)
        write_str_to_redis(m_redis_imu_entry, imu_str);
        write_img_to_redis(m_redis_img_entry, m_output_img_mat);

        // writing to the output files, after passthrough check
        if (!m_pass_through) {
            if (m_video.isOpened()) m_video.write(m_video_img_mat);
            if (m_output_imu_file.is_open()) m_output_imu_file << imu_str;
        }
        
        m_writing_ouput = false;
        m_imu_data.clear();

    } catch (...) {
        write_debug_output("ClariusProbeClient - error occured while writting to outputs");
    }

}

void ClariusProbeClient::initialize_img_handling() {

    // initializing the input containers
    m_cvt_mat = cv::Mat(CLARIUS_DEFAULT_IMG_HEIGHT, CLARIUS_DEFAULT_IMG_WIDTH, CV_8UC1);
    m_input_img_mat = cv::Mat(CLARIUS_DEFAULT_IMG_HEIGHT, CLARIUS_DEFAULT_IMG_WIDTH, CV_8UC4);
   
    // initializing the output containers
    m_output_img = QImage(m_out_img_width, m_out_img_height, QImage::Format_Grayscale8);
    m_output_img_mat = cv::Mat(m_out_img_height, m_out_img_width, CV_8UC1,
        m_output_img.bits(), m_output_img.bytesPerLine());

    // initializing the video frame cointainer
    m_video_img_mat = cv::Mat(m_out_img_height, m_out_img_width, CV_8UC3);

}

void ClariusProbeClient::configure_img_acquisition(void) {

    // defining output image dimensions (normal and preview) mode
    if (!m_stream_preview) {
        
        try {
            m_out_img_width = std::stoi((*m_config_ptr)["us_image_main_display_width"]);
            m_out_img_height = std::stoi((*m_config_ptr)["us_image_main_display_height"]);;        
        } catch (...) {
            m_out_img_width = CLARIUS_NORMAL_DEFAULT_WIDTH;
            m_out_img_height = CLARIUS_NORMAL_DEFAULT_HEIGHT;
        }
   
    } else {
        m_out_img_width = CLARIUS_PREVIEW_IMG_WIDTH;
        m_out_img_height = CLARIUS_PREVIEW_IMG_HEIGHT;
    }

}