#include "CUGNModel.h"

CUGNModel::CUGNModel(int model_id, std::string model_description, std::string model_status_entry,
	std::string redis_state_entry, std::string model_path_entry, std::string log_file_path, std::shared_ptr<ScreenRecorder> sc_p):
	MLModel(model_id, model_description, model_status_entry, redis_state_entry, model_path_entry, log_file_path){

	m_sc_p = sc_p;

	// model's default movement input
	m_default_mov_tensor = torch::zeros({1, 1, 3}, torch::TensorOptions().dtype(torch::kFloat32));

}

/*******************************************************************************
* MLMODEL OVERRIDES
******************************************************************************/

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

			// defining the model's sampling frequency + sequence length
			float sample_frequency = std::atoi((*m_config_ptr)["cugn_sample_frequency"].c_str());
			m_sampling_period_ms = round((1 / sample_frequency) * 1000);
			m_sequence_len = std::atoi((*m_config_ptr)["cugn_sequence_lenght"].c_str());

			// defining the model's image input dimensions + pixel values distribution
			m_pix_mean = std::stof((*m_config_ptr)["cugn_pixel_mean"]) * PIXEL_MAX_VALUE;
			m_pix_std_div = std::stof((*m_config_ptr)["cugn_pixel_std_div"]) * PIXEL_MAX_VALUE;
			int cugn_sc_in_h = std::atoi((*m_config_ptr)["cugn_input_h"].c_str());
			int cugn_sc_in_w = std::atoi((*m_config_ptr)["cugn_input_w"].c_str());
			m_cugn_sc_in_dims = cv::Size(cugn_sc_in_w, cugn_sc_in_h);

			// preparing the image handlingmats + automatic usimage detection
			m_us_img_detector = USImgDetector((*m_config_ptr)["cugn_us_template"]);
			m_sc_masked, m_sc_redim = cv::Mat::zeros(m_cugn_sc_in_dims, 0);

			// defining the model's starting hidden state input
			int n_gru_cells = std::atoi((*m_config_ptr)["cugn_n_gru_cells"].c_str());
			int n_gru_neurons = std::atoi((*m_config_ptr)["cugn_n_gru_neurons"].c_str());
			m_start_hx_tensor = torch::zeros({n_gru_cells, 1, n_gru_neurons}, torch::TensorOptions().dtype(torch::kFloat32));

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

/*******************************************************************************
* MODEL INFERANCE AND US IMAGE DETECTION
******************************************************************************/

void CUGNModel::eval() {

	cv::Mat sc_input;
	int input_counter = 0;
	at::Tensor hx_tensor = m_start_hx_tensor;

	detect_us_image();

    while (m_stream_status) {
		
		bool valid_preprocess = false;
		auto eval_start = std::chrono::high_resolution_clock::now();

		// preprocessing the screen recorder input
		try {

			// fetching the capture, extracting the AOI and resizing
			sc_input = m_sc_p->get_lastest_acquisition(m_sc_roi);
			cv::cvtColor(sc_input, sc_input, CV_BGRA2GRAY);
			cv::resize(sc_input, m_sc_redim, m_cugn_sc_in_dims, 0, 0, cv::INTER_AREA);
				
			// applying the US shape mask
			m_sc_redim.copyTo(m_sc_masked, m_sc_mask);

			valid_preprocess = true;

		} catch (...) {
			valid_preprocess = false;
			write_debug_output("Failed during screen reocorder input processing");
		}
		
		// feeding inputs to the model + writing to redis
		if (valid_preprocess) {

			try {
				
				// converting to tensor, normalizing and feeding the model
				at::Tensor sc_img_tensor = torch::from_blob(m_sc_masked.data,
					{1, 1, 1, m_sc_masked.rows, m_sc_masked.cols}, at::kByte).to(torch::kFloat32);
				sc_img_tensor = sc_img_tensor.sub(m_pix_mean).div(m_pix_std_div);
				
				// evaluating the model
				c10::IValue model_output = m_model.forward(
					std::vector<torch::jit::IValue>{sc_img_tensor, hx_tensor, m_default_mov_tensor});
				c10::ivalue::Tuple& model_output_tuple = model_output.toTupleRef();			
				
				// defining the next hidden state
				if (input_counter < m_sequence_len) {
					hx_tensor = model_output_tuple.elements()[1].toTensor().detach().clone();
					input_counter ++;
				} else {
					hx_tensor = m_start_hx_tensor;
					input_counter = 0;
				}
				
				// extracting the prediction + writing to redis

				at::Tensor mov_pred_tensor = model_output_tuple.elements()[0].toTensor().detach();
				float x_rot_pred = mov_pred_tensor[0][0][0].item<float>();
				float y_rot_pred = mov_pred_tensor[0][0][1].item<float>();
				float z_rot_pred = mov_pred_tensor[0][0][2].item<float>();
				
				if (m_redis_state) {
					std::string model_rot_pred_str = std::to_string(x_rot_pred) +
						"," + std::to_string(y_rot_pred) + "," + std::to_string(z_rot_pred);
					write_str_to_redis(m_redis_pred_entry, model_rot_pred_str);
				}
				
			} catch (std::exception e) {
				write_debug_output("Failed during model evaluation : " + QString::fromStdString(e.what()));
			}
			
		}

		auto eval_end = std::chrono::high_resolution_clock::now();
		auto eval_time = std::chrono::duration_cast<std::chrono::milliseconds>(eval_end - eval_start).count();
		auto thread_wait_time = m_sampling_period_ms - eval_time;
		if (thread_wait_time < 0) thread_wait_time = 0;
		std::this_thread::sleep_for(std::chrono::milliseconds(thread_wait_time));

    }

}

void CUGNModel::detect_us_image(void) {

	bool us_img_detected = false;
	write_debug_output("US image detection - start");

	while (m_stream_status && !us_img_detected) {
	
		// trying to detect a US image in the screen recorder stream
		cv::Mat sc_input = m_sc_p->get_lastest_acquisition();
		ImgDetectData detection_data = m_us_img_detector.detect(sc_input);

		if (detection_data.detected) {

			us_img_detected = true;
			m_sc_roi = detection_data.bounding_box;

			// formating the detected mask
			m_sc_mask = detection_data.mask;
			cv::resize(m_sc_mask, m_sc_mask, m_cugn_sc_in_dims, 0, 0, cv::INTER_AREA);
			cv::threshold(m_sc_mask, m_sc_mask, 0, 255, cv::THRESH_BINARY);

			// displaying the detection
			cv::rectangle(sc_input, m_sc_roi, (0, 0, 255), 3);
			QImage display_img = QImage(MODEL_DISPLAY_WIDTH, MODEL_DISPLAY_HEIGHT, QImage::Format_RGB888);
			cv::Mat display_img_mat = cv::Mat(MODEL_DISPLAY_HEIGHT, MODEL_DISPLAY_WIDTH, CV_8UC3, display_img.bits(), display_img.bytesPerLine());
			cv::resize(sc_input, display_img_mat, display_img_mat.size(), 0, 0, cv::INTER_AREA);
			emit new_us_img_detection(std::move(display_img));

			write_debug_output("US image detection : image detected");

		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(MODEL_DETECTION_DELAY_MS));
		}

	}

	write_debug_output("US image detection - end");

}