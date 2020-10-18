import argparse
import numpy as np
import matplotlib.pyplot as plt

from sonopy.gaze import GazeDataManager
from sonopy.file_management import SonoFolderManager


if __name__ == "__main__":

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Directory containing the acquisition files")
    args = parser.parse_args()

    # defining the path to the config file
    config_file_path = "/home/one_wizard_boi/Documents/Projects/MedicalUltrasound/SonoAsist/SonoAssistScripts/config.json"

    # generating saliency maps for all us video frames    
    gaze_manager = GazeDataManager(args.acquisition_dir, config_file_path)
    (saliency_map, gaze_points) = gaze_manager.generate_saliency_map(gaze_manager.gaze_data.loc[300, "OS acquisition time"])
    
    # displaying the saliency data
    for gaze_point in gaze_points:
        print(f"pos x : {int(gaze_point[0] * gaze_manager.saliency_map_width)},\
                pos y : {int(gaze_point[1] * gaze_manager.saliency_map_height)}")

    saliency_map *= 255
    img_plot = plt.imshow(saliency_map)
    plt.show()