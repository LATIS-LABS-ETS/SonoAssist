#include "MLModel.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// constructor && destructor

MLModel::MLModel(int model_id, const std::string& model_description, 
	const std::string& redis_state_entry, const std::string& model_path_entry, const std::string& log_file_path):
	m_model_id(model_id), m_model_description(model_description), m_redis_state_entry(redis_state_entry), m_model_path_entry(model_path_entry){

	if (log_file_path != "") {
		m_log_file.open(log_file_path, std::fstream::app);
	}

}

MLModel::~MLModel() {
	m_log_file.close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// getters and setters

bool MLModel::get_model_status(void) const {
	return m_model_status;
}

bool MLModel::get_redis_state(void) const {
	return m_redis_state;
}

void MLModel::set_configuration(std::shared_ptr<config_map> config_ptr) {

	m_config_ptr = config_ptr;

	// getting the redis status (is the device expected to connect to redis)
	try {
		m_redis_state = (*m_config_ptr)[m_redis_state_entry] == "true";
	} catch (...) {
		m_redis_state = false;
		write_debug_output("Failed to load redis status from config");
	}

	// loading the specified pytorch model
	try {
		m_model = torch::jit::load((*m_config_ptr)[m_model_path_entry]);
		m_model_status = true;
	} 
	catch (const c10::Error& e) {
		m_model_status = false;
		write_debug_output("Failed to load model from torch script file : " + QString::fromStdString(e.msg()));
	}
	catch (...) {
		m_model_status = false;
		write_debug_output("Failed to load model from torch script file");
	}

	m_config_loaded = true;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// redis methods

void MLModel::connect_to_redis(const std::vector<std::string>&& redis_entries) {

	try {

		// creating a new redis connection
		sw::redis::ConnectionOptions connection_options;
		connection_options.host = REDIS_ADDRESS;
		connection_options.port = REDIS_PORT;
		connection_options.socket_timeout = std::chrono::milliseconds(REDIS_TIMEOUT);
		m_redis_client_p = std::make_unique<sw::redis::Redis>(connection_options);

		// clearing the sepcified entries
		for (auto i = 0; i < redis_entries.size(); i++) {
			m_redis_client_p->del(redis_entries[i]);
		}

		m_redis_state = true;

	} catch (...) {
		m_redis_state = false;
		write_debug_output("Failed to connect to redis");
	}

}

void MLModel::disconnect_from_redis(void) {
	m_redis_client_p = nullptr;
}

void MLModel::write_str_to_redis(const std::string& redis_entry, std::string data_str) {

	if (m_redis_state && m_redis_client_p != nullptr) {

		try {
			m_redis_client_p->rpush(redis_entry, { data_str });
		} catch (...) {
			m_redis_state = false;
			write_debug_output("Failed to write (string) to redis");
		}

	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// helpers

/*
* Writes debug output to QDebug (the debug console) and the debug output window
*/
void MLModel::write_debug_output(const QString& debug_str) {

	QString out_str = QString::fromUtf8(m_model_description.c_str()) + " - " + debug_str;

	// sending msg to the application and vs debug windows
	qDebug() << "\n" + out_str;
	emit debug_output(out_str);

	// logging the message
	m_log_file << out_str.toStdString() << std::endl;

}