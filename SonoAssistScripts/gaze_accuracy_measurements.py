'''
Measures the accuracy of the eye tracker by analyzing the gaze data

The lenght of acquisition should be around 1 minute
The screen recorder should be active during the acquisition

Usage : 

    python3 gaze_accuracy_measurements.py (path to the acquisition folder)
'''

import cv2
import math
import argparse
import numpy as np
import matplotlib.pyplot as plt

from sonopy.gaze import GazeDataManager
from sonopy.video import VideoManager
from sonopy.file_management import SonoFolderManager


if __name__ == "__main__":

    # % offset from start and end off acquisition
    acquisition_offset = 0.2

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Directory containing the acquisition files")
    args = parser.parse_args()

    # defining the path to the config file
    config_file_path = "/home/one_wizard_boi/Documents/Projects/MedicalUltrasound/SonoAsist/SonoAssistScripts/config.json"

    # loading gaze data + only filtering according to speed (allowing no movements) 
    gaze_manager = GazeDataManager(args.acquisition_dir, config_file_path, filter_gaze_data=False)
    gaze_manager.filter_gaze_speed()

    # getting a frame from the full screen recording (for displaying gaze points)
    video_manager = VideoManager(gaze_manager.folder_manager["screen_rec_video"])
    screen_frame = video_manager[video_manager.frame_count // 2]

    # getting the visual target positions
    output_params = gaze_manager.folder_manager.load_output_params()
    display_targets = []
    display_targets.append((output_params["display_x"], output_params["display_y"]))
    display_targets.append((output_params["display_x"] + output_params["display_width"], output_params["display_y"]))
    display_targets.append((output_params["display_x"], output_params["display_y"] + output_params["display_height"]))
    display_targets.append((output_params["display_x"] + output_params["display_width"], output_params["display_y"] + output_params["display_height"]))

    # getting the avg px error between gaze points and their nearest target

    avg_px_error = 0

    # going through relevant gaze points
    start_gaze_i = int(acquisition_offset * gaze_manager.n_gaze_acquisitions)
    stop_gaze_i = int((1 - acquisition_offset) * gaze_manager.n_gaze_acquisitions)
    for gaze_i in range(start_gaze_i, stop_gaze_i):
    
        # getting the gaze point position (pixels)
        x_coord = int(round(gaze_manager.gaze_data.loc[gaze_i, "X"] * output_params["screen_width"]))
        y_coord = int(round(gaze_manager.gaze_data.loc[gaze_i, "Y"] * output_params["screen_height"]))

        # identifying the closest target to the point
        min_index = 0
        min_distance = output_params["screen_width"]
        for target_i in range(len(display_targets)):
            distance = math.sqrt(abs(x_coord - display_targets[target_i][0])**2 + abs(y_coord - display_targets[target_i][1])**2)
            if distance < min_distance :
                min_distance = distance
                min_index = target_i

        # accumulating the error measurement (px) and adding  the gaze point to the frame
        avg_px_error += min_distance
        cv2.circle(screen_frame, (x_coord, y_coord), 3, (255, 0, 0), 1)

    avg_px_error /= len(range(start_gaze_i, stop_gaze_i))
    
    # displaying the error measurements
    width_error = (avg_px_error/output_params["screen_width"]) * 100
    height_error = (avg_px_error/output_params["screen_height"]) * 100
    print(f"Average gaze error in pixels : {avg_px_error} px")
    print(f"Error percentage for screen width : {width_error} %")
    print(f"Error percentage for screen height : {height_error} %")

    # displaying gaze points
    cv2.imshow("Gaze points", screen_frame)
    cv2.waitKey(0)