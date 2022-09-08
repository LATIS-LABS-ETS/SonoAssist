#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <exception>

#include <QObject>
#include<QDebug>

#undef slots
#include <torch/script.h>
#define slots Q_SLOTS
#include <sw/redis++/redis++.h>

#include "SensorDevice.h"

/*
* Interface for the implementation ML models loaded from torch scripts
*/
class MLModel : public QObject {

	Q_OBJECT

	public:

		MLModel(int, const std::string&, const std::string&, const std::string&, const std::string&);
		virtual ~MLModel();

		// interface methods (virtual)
		// all interface functions must be non-bloking
		virtual void eval(void) = 0;
		virtual void start_stream(void) = 0;
		virtual void stop_stream(void) = 0;
		
		// setters and getters
		bool get_model_status(void) const;
		bool get_redis_state(void) const;
		void set_configuration(std::shared_ptr<config_map> config_ptr);

		// redis methods
		void connect_to_redis(const std::vector<std::string> && = {});
		void disconnect_from_redis(void);
		void write_str_to_redis(const std::string&, std::string);

	protected:
		void write_debug_output(const QString&);

	protected:

		int m_model_id;
		bool m_model_status = false;
		bool m_stream_status = false;
		std::string m_model_description;

		// configs vars
		bool m_config_loaded = false;
		std::string m_model_path_entry;
		std::shared_ptr<config_map> m_config_ptr;

		// redis vars
		bool m_redis_state = false;
		std::string m_redis_state_entry;
		std::unique_ptr<sw::redis::Redis> m_redis_client_p;

		std::ofstream m_log_file;
		torch::jit::script::Module m_model;

	signals:
		void debug_output(QString debug_str);

};