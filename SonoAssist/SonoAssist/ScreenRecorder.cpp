#include "ScreenRecorder.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ScreenRecorder public methods

void ScreenRecorder::connect_device() {

	// making sure requirements are filled
	if (m_config_loaded && m_sensor_used && m_output_file_loaded) {

		// getting the target window handle and bounding rectangle
        m_window_handle = GetDesktopWindow();
        if (m_window_handle != NULL) {
            
            // defining the capture rect
            GetClientRect(m_window_handle, &m_window_rc);
         
            // defining the display dimensions
            m_resized_img_width = m_window_rc.right / SR_PREVIEW_RESIZE_FACTOR;
            m_resized_img_height = m_window_rc.bottom / SR_PREVIEW_RESIZE_FACTOR;

            // initializing image handling containers
            m_capture_mat = cv::Mat(m_window_rc.bottom, m_window_rc.right, CV_8UC4);
            m_capture_cvt_mat = cv::Mat(m_window_rc.bottom, m_window_rc.right, CV_8UC3);
            m_output_img = QImage(m_resized_img_width, m_resized_img_height, QImage::Format_RGB888);
            m_output_img_mat = cv::Mat(m_resized_img_height, m_resized_img_width, 
                CV_8UC3, m_output_img.bits(), m_output_img.bytesPerLine());
            
            m_device_connected = true;
        }  

	}

	emit device_status_change(m_device_connected);

}

void ScreenRecorder::disconnect_device() {

	// making sure requirements are filled
	if (m_device_connected) {
		m_device_connected = false;
		emit device_status_change(false);
	}

}

void ScreenRecorder::start_stream() {

    if (m_device_connected && !m_device_streaming) {

        // creating a writer for the captured frames
        m_video = std::make_unique<cv::VideoWriter>(m_output_video_file_str, CV_FOURCC('M', 'J', 'P', 'G'),
            SCREEN_CAPTURE_FPS, cv::Size(m_window_rc.right, m_window_rc.bottom));

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
	
        // destroying the video writter
        m_video->release();
    }

}

void ScreenRecorder::set_output_file(std::string output_folder_path) {

    m_output_video_file_str = output_folder_path + "/screen_recorder_images.avi";
    m_output_file_loaded = true;

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// data collection function

/*
* Collects the most recent screen capture and makes it available to the main window.
* This function is meant to be executed in a seperate thread.
*/
void ScreenRecorder::collect_window_captures(void) {
  
	while (m_collect_data) {
	
        // capturing the target window
       hwnd2mat();
       cv::cvtColor(m_capture_mat, m_capture_cvt_mat, CV_BGRA2BGR);

        // in preview mode, resizing an sending the image to UI (low resolution display)
        if (m_stream_preview) {
            cv::resize(m_capture_cvt_mat, m_output_img_mat, m_output_img_mat.size(), 0, 0, cv::INTER_AREA);
            emit new_window_capture(m_output_img);
            std::this_thread::sleep_for(std::chrono::milliseconds(CAPTURE_DISPLAY_THREAD_DELAY_MS));
        }
        
        // in normal mode, write to video file
        else m_video->write(m_capture_cvt_mat);
		
	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// utility functions

/*
Converts the window's bitmap format to a cv::Mat
Source : https://stackoverflow.com/questions/14148758/how-to-capture-the-desktop-in-opencv-ie-turn-a-bitmap-into-a-mat/14167433#14167433
*/
void ScreenRecorder::hwnd2mat() {

    cv::Mat src;
    HBITMAP hbwindow;
    BITMAPINFOHEADER bi;
    HDC hwindowDC, hwindowCompatibleDC;
    int height, width, srcheight, srcwidth;

    // getting the window device context
    hwindowDC = GetDC(m_window_handle);
    hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
    SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);    

    // defining the resized image dimensions
    width = m_window_rc.right;
    height = m_window_rc.bottom;
    srcwidth = m_window_rc.right;
    srcheight = m_window_rc.bottom;
   
    // create a bitmap
    hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    // use the previously created device context with the bitmap
    SelectObject(hwindowCompatibleDC, hbwindow);

    // copy from the window device context to the bitmap device context
    StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, srcwidth, srcheight, SRCCOPY);
    GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, m_capture_mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // avoid memory leak
    DeleteObject(hbwindow);
    DeleteDC(hwindowCompatibleDC);
    ReleaseDC(m_window_handle, hwindowDC);

}
