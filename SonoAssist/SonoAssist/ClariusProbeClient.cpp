#include "ClariusProbeClient.h"

#ifdef _MEASURE_US_IMG_RATES_
 
// declaring performance measurement vars
extern int n_clarius_frames;
extern long long clarius_start, clarius_stop;
extern std::vector<long long> input_img_stamps;

#endif /*_MEASURE_US_IMG_RATES_*/

// global pointers (for the Clarius callback(s))
ClariusProbeClient* probe_client_p = nullptr;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// acquisition callback

/*
Callback function for incoming processed images (as displayed on the tablet) from the Clarius probe
Writes acquired data (IMU and images) to outputfiles and displays the images on the UI 
*/
 void new_processed_image_callback(const void* img, const ClariusProcessedImageInfo* nfo, int npos, const ClariusPosInfo* pos) {

#ifdef _MEASURE_US_IMG_RATES_

     // measuring img handling for the (N_US_FRAMES) first frames
     if (n_clarius_frames < N_US_FRAMES) {
     
         // start point for avg FPS calculation
         if (n_clarius_frames == 0) {
             clarius_start = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch()).count();
         }

         // memorising up to (N_US_FRAMES) points
         input_img_stamps[n_clarius_frames] = std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch()).count();
         
         n_clarius_frames ++;
     } 

#endif /*_MEASURE_US_IMG_RATES_*/

    // mapping the incoming image to a mat
    probe_client_p->m_input_img_mat.data = static_cast<uchar*>(const_cast<void*>(img));

    // changing color representation and resizing
    cv::cvtColor(probe_client_p->m_input_img_mat, probe_client_p->m_cvt_mat, CV_BGRA2GRAY);
    cv::resize(probe_client_p->m_cvt_mat, probe_client_p->m_output_img_mat, 
        probe_client_p->m_output_img_mat.size(), 0, 0, cv::INTER_AREA);

    // writting data to output files (csv and video), in main mode
    if (probe_client_p->get_stream_status() && !probe_client_p->get_stream_preview_status()) {

        std::string output_str = probe_client_p->get_millis_timestamp();

        if (npos) {
            output_str += "," + std::to_string(pos->gx) + "," + std::to_string(pos->gy) + "," + std::to_string(pos->gz) + "," + 
                std::to_string(pos->ax) + "," + std::to_string(pos->ay) + "," + std::to_string(pos->az) + "\n";
        } else {
            output_str += ", , , , , , \n";
        }

        probe_client_p->m_output_imu_file << output_str;
        probe_client_p->m_video->write(probe_client_p->m_output_img_mat);

    }

    // emitting the image to the UI
    emit probe_client_p->new_us_image(probe_client_p->m_output_img);

#ifdef _MEASURE_US_IMG_RATES_

    // stop point for avg FPS calculation
    if (n_clarius_frames == N_US_FRAMES) {
        clarius_stop = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        n_clarius_frames ++;
    }

#endif /*_MEASURE_US_IMG_RATES_*/

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ClariusProbeClient public methods

void ClariusProbeClient::connect_device() {

    // making sure requirements are filled
    if (m_config_loaded && m_sensor_used && m_output_file_loaded) {

        disconnect_device();

        // global instance pointer (for the clarius callbacks)
        if (probe_client_p == nullptr) probe_client_p = this;

        // mapping the listener's events to Qt application level events
        if (clariusInitListener(0, nullptr, 
                QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString().c_str(),
                new_processed_image_callback, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                CLARIUS_NORMAL_DEFAULT_WIDTH, CLARIUS_NORMAL_DEFAULT_HEIGHT) == 0)
        {
            m_device_connected = true;
            qDebug() << "\nClariusProbeClient - successfully connected\n";
        } 
        
        // failure to map events
        else {
            qDebug() << "\nClariusProbeClient - failed to connect\n";
        }

    }

    emit device_status_change(m_device_connected);

}

void ClariusProbeClient::disconnect_device() {
    
    if (m_device_connected) { 
        clariusDestroyListener();
        m_device_connected = false;
        emit device_status_change(false);
    }

}

void ClariusProbeClient::start_stream() {
    
    // making sure requirements are filled
    if (m_device_connected && !m_device_streaming) {
        
        // preparing for image handling
        configure_img_acquisition();
        initialize_img_handling();

        // preparing the writing of data
        set_output_file(m_output_folder_path);
        m_output_imu_file.open(m_output_imu_file_str, std::fstream::app);
        m_video = std::make_unique<cv::VideoWriter>(m_output_video_file_str, CV_FOURCC('M', 'J', 'P', 'G'),
            CLARIUS_VIDEO_FPS, cv::Size(m_out_img_width, m_out_img_height), false);

        // connecting to the probe events
        try {
            if (!(clariusConnect((*m_config_ptr)["us_probe_ip_address"].c_str(), 
                    std::stoi((*m_config_ptr)["us_probe_udp_port"]), nullptr) < 0)) 
            {    
                m_device_streaming = true;
            }  
        } catch (...) {}

        if (m_device_streaming) qDebug() << "\nClariusProbeClient - successfully started the acquisition\n";
        else qDebug() << "\nClariusProbeClient - failed to start the acquisition\n";

    }

}

void ClariusProbeClient::stop_stream() {
    
    if (m_device_streaming) {

        // stopping the acquisition
        clariusDisconnect(nullptr);
        m_device_streaming = false;

        // closing the outputs
        m_video->release();
        m_output_imu_file.close();

        qDebug() << "\nClariusProbeClient - stoped the acquisition\n";
    }

}

void ClariusProbeClient::set_output_file(std::string output_folder_path) {

    try {

        m_output_folder_path = output_folder_path;

        // defining the output file path
        m_output_imu_file_str = output_folder_path + "/clarius_imu.csv";
        m_output_video_file_str = output_folder_path + "/clarius_images.avi";
        if (m_output_imu_file.is_open()) m_output_imu_file.close();

        // writing the output file header
        m_output_imu_file.open(m_output_imu_file_str);
        m_output_imu_file << "Time (ms),gx,gy,gz,ax,ay,az" << std::endl;
        m_output_imu_file.close();

        m_output_file_loaded = true;

    } catch (...) {
        qDebug() << "\ClariusProbeClient - error occured while setting the output file";
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// helper functions

void ClariusProbeClient::initialize_img_handling() {

    // initializing the input containers
    m_input_img_mat = cv::Mat(CLARIUS_DEFAULT_IMG_HEIGHT, CLARIUS_DEFAULT_IMG_WIDTH, CV_8UC4);
    
    // initializing the color conversion containers
    m_cvt_mat = cv::Mat(CLARIUS_DEFAULT_IMG_HEIGHT, CLARIUS_DEFAULT_IMG_WIDTH, CV_8UC1);

    // initializing the output containers
    m_output_img = QImage(m_out_img_width, m_out_img_height, QImage::Format_Grayscale8);
    m_output_img_mat = cv::Mat(m_out_img_height, m_out_img_width, CV_8UC1,
        m_output_img.bits(), m_output_img.bytesPerLine());

}

/*
Loads the configurations for the generation of output images
*/
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