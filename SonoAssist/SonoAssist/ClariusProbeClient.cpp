#include "SonoAssist.h"
#include "ClariusProbeClient.h"

// defining reception buffers for (image) and (IMU data) deep copies
static ClariusPosInfo _imu;
static std::vector<char> _image;
static std::vector<char> _rawImage;
const int imu_data_size = sizeof(ClariusPosInfo);

// global pointers (for the Clarius callback(s))
static ClariusProbeClient* probe_client_p = nullptr;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////// ClariusProbeClient public methods

void ClariusProbeClient::connect_device() {

    // making sure requirements are filled
    if (m_config_loaded && m_sensor_used) {

        // making sure the device is disconnected
        disconnect_device();

        // updating the global instance pointer (for the clarius callbacks)
        if (probe_client_p == nullptr) probe_client_p = this;

        auto new_processed_image_callback = [](const void* img, const ClariusProcessedImageInfo* nfo, int npos, const ClariusPosInfo* pos) {
   
            // deep copy of the image data since we have to post the event
            int data_size = nfo->width * nfo->height * (nfo->bitsPerPixel / 8);
            if (_image.size() < data_size) _image.resize(data_size);
            memcpy(_image.data(), img, data_size);

            // copying the IMU data
            if (npos) memcpy(&_imu, pos, imu_data_size);

            // emitting an image ...

        };

        // mapping the listener's events to Qt application level events
        if (!clariusInitListener(0, nullptr, 
                QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString().c_str(),
                new_processed_image_callback, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                CLARIUS_NORMAL_IMG_WIDTH, CLARIUS_NORMAL_IMG_HEIGHT))
        {
            m_device_connected = true;
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
        
        // connecting to the probe events
        try {
            if (clariusConnect((*m_config_ptr)["us_probe_ip_address"].c_str(), 
                    std::stoi((*m_config_ptr)["us_probe_udp_port"]), nullptr) < 0) 
            {    
                m_device_streaming = true;
            }
        } catch (...) {}

    }

}

void ClariusProbeClient::stop_stream() {
    
    if (m_device_streaming) {
        clariusDisconnect(nullptr);
        m_device_streaming = false;
    }

}