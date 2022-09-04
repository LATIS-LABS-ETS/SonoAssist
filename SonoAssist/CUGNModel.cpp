#include "CUGNModel.h"

CUGNModel::CUGNModel(int model_id, const std::string& model_description, const std::string& redis_state_entry, 
	const std::string& model_path_entry, const std::string& log_file_path, std::shared_ptr<ScreenRecorder> sc_p):
	MLModel(model_id, model_description, redis_state_entry, model_path_entry, log_file_path){

	m_sc_p = sc_p;

	// model's default movement input
	m_default_mov_tensor = torch::zeros({1, 1, 3}, torch::TensorOptions().dtype(torch::kFloat32));

}

void CUGNModel::start_stream(void) {

	bool valid_params = true;

	if (m_config_loaded && m_model_status) {
	
		// connecting to redis
		if (m_redis_state) {
			m_redis_pred_entry = (*m_config_ptr)["cugn_redis_entry"];
			connect_to_redis({m_redis_pred_entry});
		}

		// loading model pipeline params
		try {

			// defining the model's samlping frequency
			float sample_frequency = std::atoi((*m_config_ptr)["cugn_sample_frequency"].c_str());
			m_sampling_delay = round((1 / sample_frequency) * 1000);

			// defining the US image bounding box
			int sc_roi_x = std::atoi((*m_config_ptr)["cugn_sc_bbox_x"].c_str());
			int sc_roi_y = std::atoi((*m_config_ptr)["cugn_sc_bbox_y"].c_str());
			int sc_roi_w = std::atoi((*m_config_ptr)["cugn_sc_bbox_w"].c_str());
			int sc_roi_h = std::atoi((*m_config_ptr)["cugn_sc_bbox_h"].c_str());
			m_sc_roi = cv::Rect(sc_roi_x, sc_roi_y, sc_roi_w, sc_roi_h);

			// defining the model's image input dimensions + pixel values distribution
			m_pix_mean = std::stof((*m_config_ptr)["cugn_pixel_mean"]);
			m_pix_std_div = std::stof((*m_config_ptr)["cugn_pixel_std_div"]);
			int cugn_sc_in_h = std::atoi((*m_config_ptr)["cugn_input_h"].c_str());
			int cugn_sc_in_w = std::atoi((*m_config_ptr)["cugn_input_w"].c_str());
			m_cugn_sc_in_dims = cv::Size(cugn_sc_in_w, cugn_sc_in_h);

			// loading / preparing the US image shape mask
			m_sc_masked, m_sc_redim = cv::Mat::zeros(m_cugn_sc_in_dims, 0);
			m_sc_mask = cv::imread((*m_config_ptr)["cugn_sc_mask_path"], cv::IMREAD_GRAYSCALE);
			cv::resize(m_sc_mask, m_sc_mask, m_cugn_sc_in_dims, 0, 0, cv::INTER_AREA);
			cv::threshold(m_sc_mask, m_sc_mask, 0, 255, cv::THRESH_BINARY);

			// defining the model's starting hidden state input
			int n_gru_cells = std::atoi((*m_config_ptr)["cugn_n_gru_cells"].c_str());
			int n_gru_neurons = std::atoi((*m_config_ptr)["cugn_n_gru_neurons"].c_str());
			m_hx_tensor = torch::zeros({n_gru_cells, 1, n_gru_neurons}, 
				torch::TensorOptions().dtype(torch::kFloat32));

		} catch(...) {
			valid_params = false;
			write_debug_output("Invalid model pipeline parameters");
		}

		// launching the model evaluation
		if (valid_params) {
			m_stream_status = true;
			m_eval_thread = std::thread(&CUGNModel::eval, this);
		}
		
	}

}

void CUGNModel::stop_stream(void) {
    
    if (m_stream_status) {
		m_stream_status = false;
		m_eval_thread.join();
		disconnect_from_redis();
    }

}

void CUGNModel::eval() {

	cv::Mat sc_input;

    while (m_stream_status) {
		
		// preprocessing the screen recorder input
		bool valid_preprocess = true;
		try {

			// fetching the capture, extracting the AOI and resizing
			sc_input = m_sc_p->get_lastest_acquisition();
			sc_input = sc_input(m_sc_roi);
			cv::cvtColor(sc_input, sc_input, CV_BGRA2GRAY);
			cv::resize(sc_input, m_sc_redim, m_cugn_sc_in_dims, 0, 0, cv::INTER_AREA);
			// applying the US shape mask
			m_sc_redim.copyTo(m_sc_masked, m_sc_mask);

		} catch (...) {
			valid_preprocess = false;
			write_debug_output("Failed during screen reocorder input processing");
		}
		
		// feeding inputs to the model + writing to redis
		if (valid_preprocess) {

			try {
				
				// converting to tensor, normalizing and feeding the model
				at::Tensor sc_img_tensor = torch::from_blob(m_sc_masked.data,
					{1, 1, 1, m_sc_masked.rows, m_sc_masked.cols},
					torch::TensorOptions().dtype(torch::kFloat32));
				sc_img_tensor = sc_img_tensor.div(PIXEL_MAX_VALUE).sub(m_pix_std_div).div(m_pix_mean);
				
				// evaluating the model + extracting predicted data
				c10::IValue model_output = m_model.forward(
					std::vector<torch::jit::IValue>{sc_img_tensor, m_hx_tensor, m_default_mov_tensor});
				c10::ivalue::Tuple& model_output_tuple = model_output.toTupleRef();			
				m_hx_tensor = model_output_tuple.elements()[1].toTensor().detach().clone();
				at::Tensor mov_pred_tensor = model_output_tuple.elements()[0].toTensor().detach();
				float x_rot_pred = mov_pred_tensor[0][0][0].item<float>();
				float y_rot_pred = mov_pred_tensor[0][0][1].item<float>();
				float z_rot_pred = mov_pred_tensor[0][0][2].item<float>();
				
				if (m_redis_state) {
					std::string model_rot_pred_str = "[" +
						std::to_string(x_rot_pred) + ", " +
						std::to_string(y_rot_pred) + ", " +
						std::to_string(z_rot_pred) + "]";
					write_str_to_redis(m_redis_pred_entry, model_rot_pred_str);
				}
				
			} catch (std::exception e) {
				write_debug_output("Failed during model evaluation : " + QString::fromStdString(e.what()));
			}
			
		}

        std::this_thread::sleep_for(std::chrono::milliseconds(m_sampling_delay));

    }

}