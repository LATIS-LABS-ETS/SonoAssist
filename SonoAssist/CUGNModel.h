#include <math.h> 
#include <string>
#include <memory>
#include <thread>
#include <chrono>

#undef slots
#include <torch/script.h>
#define slots Q_SLOTS
#include <opencv2/opencv.hpp>

#include <QString>

#include "MLModel.h"
#include "ScreenRecorder.h"

#define PIXEL_MAX_VALUE 255
#define MODEL_DETECTION_DELAY_MS 500

/*
* Class for the real time evaluation of the Cardiac Ultrasound GuideNet (CUGN) model
* Model inputs :
*	xt: The (t)th captured ultrasound image
*	ht-1: The previous hidden state (for the GRU)
* Model outputs :
*	ot: The predicted absolute value of the rotation, in Euler angles and expressed in radians, between the probe's
*		orientation at image (xt) and its orientation at the standard view (xf).
*	at: The predicted visual saliency map associated with (xt)
*/
class CUGNModel : public MLModel {

	public:

		CUGNModel(int model_id, std::string model_description, std::string model_status_entry,
			std::string redis_state_entry, std::string model_path_entry, std::string log_file_path, std::shared_ptr<ScreenRecorder> sc_p);

		void start_stream(void) override;
		void stop_stream(void) override;

	private:

		/**
		* Executes the prediction pipeline with the following steps:
		*	1) US image position detection via (detect_us_image)
		*	2) Image capture and processing: cropping, color conversion, resizing & masking
		*	3) Model inferance
		* This method is meant to run in a seperate thread
		*/
		void eval(void);

		/**
		* Deploys automatic US shell shape detection
		*/
		void detect_us_image(void);

		std::thread m_eval_thread;
		int m_sampling_period_ms = 100;
		std::shared_ptr<ScreenRecorder> m_sc_p = nullptr;

		// screen recorder img preprocessing
		
		USImgDetector m_us_img_detector;
		
		cv::Rect m_sc_roi;
		cv::Size m_cugn_sc_in_dims;
		cv::Mat m_sc_mask, m_sc_masked, m_sc_redim;
		
		int m_sequence_len = 0;
		float m_pix_mean, m_pix_std_div = 0;
		
		// model inputs
		at::Tensor m_start_hx_tensor, m_default_mov_tensor;

		// redis vars
		std::string m_redis_pred_entry;

};