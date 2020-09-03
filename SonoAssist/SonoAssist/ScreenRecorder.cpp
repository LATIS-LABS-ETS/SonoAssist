#include "ScreenRecorder.h"

void ScreenRecorder::connect_device() {

	// making sure requirements are filled
	if (m_config_loaded && m_sensor_used) {

		// getting the target window handle and bounding rectangle
        m_window_handle = GetDesktopWindow();
        if (m_window_handle != NULL) {
            GetClientRect(m_window_handle, &m_window_rc);
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

	// making sure requirements are filled
	if (m_device_connected && !m_device_streaming) {
	
		// launching the acquisition thread
		m_collect_data = true;
		m_collection_thread = std::thread(&ScreenRecorder::collect_window_captures, this);
		m_device_streaming = true;

	}

}

void ScreenRecorder::stop_stream() {

    // making sure requirements are filled
	if (m_device_streaming) {
		
		// stopping the data collection thread
		m_collect_data = false;
		m_collection_thread.join();

		m_device_streaming = false;

	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// collection function

/*
* Collects the most recent screen capture and makes it available to the main window.
* This function is meant to be executed in a seperate thread.
*/
void ScreenRecorder::collect_window_captures(void) {
  
    // defining QImage for writting
    void* frame_data_p = nullptr;
    int img_height = m_window_rc.bottom / CAPTURE_DISPLAY_RESIZE_FACTOR;
    int img_width = m_window_rc.right / CAPTURE_DISPLAY_RESIZE_FACTOR;
    QImage q_image(img_width, img_height, QImage::Format_RGB888);

	while (m_collect_data) {
	
        // capturing the target window
        cv::Mat window_capture = hwnd2mat();
        cv::cvtColor(window_capture, window_capture, CV_BGRA2BGR);

        // mapping an empty QImage's data with an empty cv:Map's data
        // changing captured image format and writing data to the QImage
        cv::Mat resized_image(img_height, img_width, CV_8UC3, q_image.bits(), q_image.bytesPerLine());
        cv::resize(window_capture, resized_image, resized_image.size(), 0, 0, cv::INTER_AREA);
       
		// emiting the capture and waiting
        emit new_window_capture(std::move(q_image.copy()));
		std::this_thread::sleep_for(std::chrono::milliseconds(CAPTURE_DISPLAY_THREAD_DELAY_MS));
	
	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// utility functions

/*
Converts the window's bitmap format to a cv::Mat
Source : https://stackoverflow.com/questions/14148758/how-to-capture-the-desktop-in-opencv-ie-turn-a-bitmap-into-a-mat/14167433#14167433
*/
cv::Mat ScreenRecorder::hwnd2mat() {

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
   
    // creating an empty cv::Mat with the proper dimensions and chanels
    src.create(height, width, CV_8UC4);

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
    GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // avoid memory leak
    DeleteObject(hbwindow);
    DeleteDC(hwindowCompatibleDC);
    ReleaseDC(m_window_handle, hwindowDC);

    return src;
}
