import cv2
import argparse
import numpy as np

from sonopy.gaze import GazeDataManager
from sonopy.clarius import ClariusDataManager

if __name__ == "__main__":

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Directory containing the acquisition files")
    args = parser.parse_args()

    # defining required paths
    acquisition_dir_path = args.acquisition_dir
    config_file_path = "/home/one_wizard_boi/Documents/Projects/MedicalUltrasound/SonoAsist/SonoAssistScripts/config.json"
    output_video_path = "/home/one_wizard_boi/Documents/Projects/MedicalUltrasound/SonoAsist/SonoAssistScripts/us_video.avi"

    # loading acquisition data
    clarius_manager = ClariusDataManager(acquisition_dir_path)
    gaze_manager = GazeDataManager(acquisition_dir_path, config_file_path)

    # defining image handling vars
    alpha = 0.7
    beta = 1 - alpha
    intensity_factor = 17000
    us_display_size = (gaze_manager.config_manager["display_width"], gaze_manager.config_manager["display_height"])
    video_output = cv2.VideoWriter(output_video_path, cv2.VideoWriter_fourcc('M','J','P','G'), 20, us_display_size)

    # getting the first valid gaze point timestamp + coresponding clarius data index
    first_gaze_time = gaze_manager.gaze_data.loc[0, gaze_manager.os_acquisition_time]
    first_clarius_index = clarius_manager.get_nearest_index(first_gaze_time)

    # progress vars
    progress_percentage = 0.1

    # going through clarius acquisitions
    for data_i in range(first_clarius_index, clarius_manager.n_acquisitions):
        
        # processing clarius data associated to valid gaze data
        saliency_map = gaze_manager.generate_saliency_map(clarius_manager.clarius_df.loc[data_i, "Display OS time"])
        if saliency_map is not None:

            # getting the US image and motion data
            clarius_data = clarius_manager.get_clarius_data(data_i)
            if clarius_data is not None:

                # processing the saliency map and overlaying ti with the US image
                saliency_map = cv2.applyColorMap((saliency_map * intensity_factor).astype(np.uint8), cv2.COLORMAP_JET)
                saliency_map = cv2.resize(saliency_map, us_display_size)
                output_img = cv2.addWeighted(clarius_data[0], alpha, saliency_map, beta, 0.0)

                video_output.write(output_img)

        # progress display
        if data_i >= (clarius_manager.n_acquisitions * progress_percentage):
            progress = int(progress_percentage * 100)
            progress_percentage += 0.1
            print(f"Training data generation progress : {progress} %")

    video_output.release()