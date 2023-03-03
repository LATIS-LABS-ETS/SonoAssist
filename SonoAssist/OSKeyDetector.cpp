#include "OSKeyDetector.h"

/*******************************************************************************
* SENSOR DEVICE OVERRIDES
******************************************************************************/

void OSKeyDetector::connect_device(void) {

    if (m_config_loaded && m_sensor_used) {
        m_device_connected = true;
    }

    emit device_status_change(m_device_id, m_device_connected);

}

void OSKeyDetector::disconnect_device(void) {
 
    m_device_connected = false;
    emit device_status_change(m_device_id, false);

}

void OSKeyDetector::start_stream() {

    if (m_device_connected && !m_device_streaming) {

        // launching the key detection thread
        m_listen = true;
        m_listener_thread = std::thread(&OSKeyDetector::detect_keys, this);

        // once the acquisition process is launched, changing the sensor's acquisition state 
        m_device_streaming = true;

    }

}

void OSKeyDetector::stop_stream() {

    if (m_device_streaming) {

        // stopping the data collection thread
        m_listen = false;
        m_listener_thread.join();

        // once the acquisition process is stoped, changing the sensor's acquisition state 
        m_device_streaming = false;

    }

}

/*******************************************************************************
* DATA COLLECTION FUNCTIONS
******************************************************************************/

void OSKeyDetector::detect_keys() {

    int input_cycle = 0;
    int input_cycle_offset = 15;

    while (m_listen) {

        if (input_cycle == 0) {
        
            // looking for a key press in the monitored keys 
            // https://stackoverflow.com/questions/41600981/how-do-i-check-if-a-key-is-pressed-on-c
            int key = OS_NO_KEY;
            if (GetKeyState('A') & 0x8000) key = OS_A_KEY;
            else if (GetKeyState('D') & 0x8000) key = OS_D_KEY;
        
            // emitting to the main window once a key press is detected
            if (key != OS_NO_KEY) {
                input_cycle = input_cycle_offset;
                emit key_detected(key);
            }

        }

        else if (input_cycle > 0) {
            input_cycle--;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        
    }

}