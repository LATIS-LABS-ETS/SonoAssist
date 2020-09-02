#include "SonoAssist.h"
#include "ClariusProbeClient.h"

// defining reception buffers for image deep copies
static std::vector<char> _image;
static std::vector<char> _rawImage;
extern SonoAssist* app_window_p;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ClariusProbeClient public methods

void ClariusProbeClient::connect_device() {

    // making sure requirements are filled
    if (m_config_loaded && m_sensor_used) {

        // making sure the device is disconnected
        disconnect_device();

        // generating application level events from probe events
        // source : https://github.com/clariusdev/listener/blob/master/src/example/qt/main.cpp
        if (!clariusInitListener(0, nullptr, QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString().c_str(),
            // new image callback
            [](const void* img, const ClariusProcessedImageInfo* nfo, int, const ClariusPosInfo*)
            {
                // we need to perform a deep copy of the image data since we have to post the event (yes this happens a lot with this api)
                size_t sz = nfo->width * nfo->height * (nfo->bitsPerPixel / 8);
                if (_image.size() < sz)
                    _image.resize(sz);
                memcpy(_image.data(), img, sz);

                if (app_window_p != nullptr) {
                    QApplication::postEvent(app_window_p, new event::Image(_image.data(), nfo->width, nfo->height, nfo->bitsPerPixel));
                }

            },
            // new raw image callback
            [](const void* img, const ClariusRawImageInfo* nfo, int, const ClariusPosInfo*)
            {
                // we need to perform a deep copy of the image data since we have to post the event (yes this happens a lot with this api)
                size_t sz = nfo->lines * nfo->samples * (nfo->bitsPerSample / 8);
                if (_rawImage.size() < sz)
                    _rawImage.resize(sz);
                memcpy(_rawImage.data(), img, sz);

                if (app_window_p != nullptr) {
                    QApplication::postEvent(app_window_p, new event::PreScanImage(_rawImage.data(), nfo->lines, nfo->samples, nfo->bitsPerSample, nfo->jpeg));
                }
            },
            // freeze state change callback
            [](int frozen)
            {
                if (app_window_p != nullptr) {
                    // post event here, as the gui (statusbar) will be updated directly, and it needs to come from the application thread
                    QApplication::postEvent(app_window_p, new event::Freeze(frozen ? true : false));
                }
            },
            // button press callback
            [](int btn, int clicks)
            {
                // post event here, as the gui (statusbar) will be updated directly, and it needs to come from the application thread
                QApplication::postEvent(app_window_p, new event::Button(btn, clicks));
            },
            // download progress state change callback
            [](int progress)
            {
                if (app_window_p != nullptr) {
                    // post event here, as the gui (proress bar) will be updated directly, and it needs to come from the application thread
                    QApplication::postEvent(app_window_p, new event::Progress(progress));
                }
            },
            // error message callback
            [](const char* err)
            {
                if (app_window_p != nullptr) {
                    // post event here, as the gui (statusbar) will be updated directly, and it needs to come from the application thread
                    QApplication::postEvent(app_window_p, new event::Error(err));
                }
            },
            nullptr, PROBE_DISPLAY_WIDTH, PROBE_DISPLAY_HEIGHT) != 0)
        {
            m_device_connected = true;
        }

    }

    emit device_status_change(m_device_connected);

}

void ClariusProbeClient::disconnect_device() {
    
    // making sure requirements are filled
    if (m_device_connected) { 
        clariusDestroyListener();
        m_device_connected = false;
        emit device_status_change(false);
    }

}

void ClariusProbeClient::start_stream() {
    
    // making sure requirements are filled
    if (m_device_connected && !m_device_streaming) {
        m_device_streaming = true;
    }

}

void ClariusProbeClient::stop_stream() {
    
    // making sure requirements are filled
    if (m_device_streaming) {
        m_device_streaming = false;
    }

}