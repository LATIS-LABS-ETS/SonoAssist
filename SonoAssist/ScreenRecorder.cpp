#include "ScreenRecorder.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ScreenRecorder public methods

ScreenRecorder::ScreenRecorder(int device_id, const std::string& device_description, const std::string& redis_state_entry, const std::string& log_file_path)
    : SensorDevice(device_id, device_description, redis_state_entry, log_file_path) 
{

    int height, width, srcheight, srcwidth;

    // getting the window handle and bounding rectangle
    m_window_handle = GetDesktopWindow();
    GetClientRect(m_window_handle, &m_window_rc);

    // getting the window device context
    m_hwindowDC = GetDC(m_window_handle);
    m_hwindowCompatibleDC = CreateCompatibleDC(m_hwindowDC);
    SetStretchBltMode(m_hwindowCompatibleDC, COLORONCOLOR);

    // defining the resized image dimensions
    width = m_window_rc.right;
    height = m_window_rc.bottom;
    srcwidth = m_window_rc.right;
    srcheight = m_window_rc.bottom;

    // create a bitmap to hold the window content
    m_hbwindow = CreateCompatibleBitmap(m_hwindowDC, width, height);
    m_bi.biSize = sizeof(BITMAPINFOHEADER);
    m_bi.biWidth = width;
    m_bi.biHeight = -height;
    m_bi.biPlanes = 1;
    m_bi.biBitCount = 32;
    m_bi.biCompression = BI_RGB;
    m_bi.biSizeImage = 0;
    m_bi.biXPelsPerMeter = 0;
    m_bi.biYPelsPerMeter = 0;
    m_bi.biClrUsed = 0;
    m_bi.biClrImportant = 0;

    // use the previously created device context with the bitmap
    SelectObject(m_hwindowCompatibleDC, m_hbwindow);

}

ScreenRecorder::~ScreenRecorder() {

    // releasing the window capture resources
    DeleteObject(m_hbwindow);
    DeleteDC(m_hwindowCompatibleDC);
    ReleaseDC(m_window_handle, m_hwindowDC);

}

void ScreenRecorder::connect_device() {

	if (m_config_loaded && m_sensor_used) {

        // defining the display dimensions
        int preview_img_width = m_window_rc.right / SR_PREVIEW_RESIZE_FACTOR;
        int preview_img_height = m_window_rc.bottom / SR_PREVIEW_RESIZE_FACTOR;

        // defining the redis img dimensions
        int redis_img_width = m_window_rc.right / REDIS_RESIZE_FACTOR;
        int redis_img_height = m_window_rc.bottom / REDIS_RESIZE_FACTOR;

        // initializing image handling containers
        m_capture_mat = cv::Mat(m_window_rc.bottom, m_window_rc.right, CV_8UC4);
        m_capture_cvt_mat = cv::Mat(m_window_rc.bottom, m_window_rc.right, CV_8UC3);
        m_preview_img = QImage(preview_img_width, preview_img_height, QImage::Format_RGB888);
        m_preview_img_mat = cv::Mat(preview_img_height, preview_img_width,
            CV_8UC3, m_preview_img.bits(), m_preview_img.bytesPerLine());
        m_redis_img_mat = cv::Mat(redis_img_height, redis_img_width, CV_8UC3);

        m_device_connected = true;
	}

	emit device_status_change(m_device_id, m_device_connected);

}

void ScreenRecorder::disconnect_device() {

	m_device_connected = false;
	emit device_status_change(m_device_id, false);

}

void ScreenRecorder::start_stream() {

    if (m_device_connected && !m_device_streaming && m_output_file_loaded) {

        // opening output files
        m_output_index_file.open(m_output_index_file_str, std::fstream::app);
        m_video = cv::VideoWriter(m_output_video_file_str, CV_FOURCC('M', 'J', 'P', 'G'),
            SCREEN_CAPTURE_FPS, cv::Size(m_window_rc.right, m_window_rc.bottom));

        // connecting to redis (if redis enabled)
        if (m_redis_state) {
            m_redis_img_entry = (*m_config_ptr)["sc_img_redis_entry"];
            m_redis_rate_div = std::atoi((*m_config_ptr)["sc_redis_rate_div"].c_str());
            connect_to_redis({ m_redis_img_entry });
        }
        
        // launching the acquisition thread
        m_collect_data = true;
	    m_collection_thread = std::thread(&ScreenRecorder::collect_window_captures, this);
	    m_device_streaming = true;

    }

}

void ScreenRecorder::stop_stream() {
    
	if (m_device_streaming) {

        // stopping the data collection thread
		m_collect_data = false;
		m_collection_thread.join();
		m_device_streaming = false;
	
        // closing output files
        m_video.release();
        m_output_index_file.close();
        disconnect_from_redis();

    }

}

void ScreenRecorder::set_output_file(const std::string& output_folder_path) {

    try {

        m_output_folder_path = output_folder_path;

        // defining the output video path
        m_output_video_file_str = output_folder_path + "/screen_recorder_images.avi";

        // defining the output index file
        m_output_index_file_str = output_folder_path + "/screen_recorder_data.csv";
        if (m_output_index_file.is_open()) m_output_index_file.close();

        // writing the output index file header
        m_output_index_file.open(m_output_index_file_str);
        m_output_index_file << "Time (us)" << std::endl;
        m_output_index_file.close();

        m_output_file_loaded = true;

    }
    catch (...) {
        write_debug_output("ClariusProbeClient - error occured while setting the output file");
    }

}

cv::Mat ScreenRecorder::get_lastest_acquisition(void) {

    cv::Mat latest_capture;

    m_capture_mtx.lock();
    latest_capture = m_capture_cvt_mat.clone();
    m_capture_mtx.unlock();

    return latest_capture;

}

void ScreenRecorder::get_screen_dimensions(int& screen_width, int& screen_height) const {
    screen_width = m_window_rc.right;
    screen_height = m_window_rc.bottom;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// data collection function

/*
* Collects the most recent screen capture and makes it available to the main window.
* This function is meant to be executed in a seperate thread.
*/
void ScreenRecorder::collect_window_captures(void) {
  
	while (m_collect_data) {
	
        // performing the window capture : copy from the window device context to the bitmap device context
        // Source: https://stackoverflow.com/questions/14148758/how-to-capture-the-desktop-in-opencv-ie-turn-a-bitmap-into-a-mat/14167433#14167433
        StretchBlt(m_hwindowCompatibleDC, 0, 0, m_window_rc.right, m_window_rc.bottom, m_hwindowDC, 0, 0, m_window_rc.right, m_window_rc.bottom, SRCCOPY);
        GetDIBits(m_hwindowCompatibleDC, m_hbwindow, 0, m_window_rc.bottom, m_capture_mat.data, (BITMAPINFO*)&m_bi, DIB_RGB_COLORS);
        
        // color conversion -> filling (m_capture_cvt_mat) the screen capture's final form
        m_capture_mtx.lock();
        cv::cvtColor(m_capture_mat, m_capture_cvt_mat, CV_BGRA2BGR);
        m_capture_mtx.unlock();

        // in preview mode, resizing an sending the image to UI (low resolution display)
        if (m_stream_preview) {
            cv::resize(m_capture_cvt_mat, m_preview_img_mat, m_preview_img_mat.size(), 0, 0, cv::INTER_AREA);
            emit new_window_capture(m_preview_img);
            std::this_thread::sleep_for(std::chrono::milliseconds(CAPTURE_DISPLAY_THREAD_DELAY_MS));
        }
        
        // in normal mode, write to video and index file + redis
        else {

            if (m_redis_state) {
                cv::resize(m_capture_cvt_mat, m_redis_img_mat, m_redis_img_mat.size(), 0, 0, cv::INTER_AREA);
                cv::cvtColor(m_redis_img_mat, m_redis_img_mat, CV_BGRA2GRAY);
                write_img_to_redis(m_redis_img_entry, m_redis_img_mat);
            }
           
            // write to file
            if (!m_pass_through) {
                m_video.write(m_capture_cvt_mat);
                m_output_index_file << get_micro_timestamp() << "\n";
            }
            
        }

	}

}