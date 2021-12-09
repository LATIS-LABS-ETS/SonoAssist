#include "MetaWearArray.h"


void MetaWearArray::connect_device() {

	// trying to connect to devices
	bool connection_success = true;
	for (auto i(0); i < m_clients.size(); i++) {
		
		m_clients[i].connect_device();

		int n_tries = 0;
		int max_tries = 100;

		while (m_clients[i].get_connection_status()) {

			n_tries++;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		
			if (n_tries > max_tries) {
				connection_success *= false;
			}

		}

	}

	m_device_connected = connection_success;
	emit device_status_change(m_device_id, m_device_connected);

}

void MetaWearArray::disconnect_device() {

	for (auto i(0); i < m_clients.size(); i++) {
		m_clients[i].disconnect_device();
	
	}

	m_device_connected = false;
	emit device_status_change(m_device_id, m_device_connected);
		
}

void MetaWearArray::set_output_file(std::string output_folder) {

	for (auto i(0); i < m_clients.size(); i++) {
		m_clients[i].set_output_file(output_folder);

	}


}



