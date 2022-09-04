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

/*
Class for the real time evaluation of the CUGN model
*/
class CUGNModel : public MLModel {

	public:

		CUGNModel(int, const std::string&, const std::string&, const std::string&, 
			const std::string&, std::shared_ptr<ScreenRecorder>);

		// interface methods
		void eval(void);
		void start_stream(void);
		void stop_stream(void);

	private:

		std::thread m_eval_thread;
		int m_sampling_period_ms = 100;
		std::shared_ptr<ScreenRecorder> m_sc_p = nullptr;

		// screen recorder img preprocessing
		cv::Rect m_sc_roi;
		cv::Size m_cugn_sc_in_dims;
		cv::Mat m_sc_mask, m_sc_masked, m_sc_redim;
		float m_pix_mean, m_pix_std_div = 0;

		// model inputs
		at::Tensor m_hx_tensor, m_default_mov_tensor;

		// redis vars
		std::string m_redis_pred_entry;

};