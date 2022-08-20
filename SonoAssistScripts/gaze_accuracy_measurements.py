'''
Measures the accuracy of the eye tracker by analyzing the gaze data

Usage
-----
python3 gaze_accuracy_measurements.py (path to the acquisition folder)
'''

import cv2
import math
import argparse
import numpy as np
import matplotlib.pyplot as plt

from sonopy.video import VideoManager
from sonopy.gaze import GazeDataManager


if __name__ == "__main__":

    # defining the path to the config file
    config_file_path = "./config.json"

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Directory containing the acquisition files")
    args = parser.parse_args()

    # loading gaze data + only filtering according to speed (allowing no movements) 
    gaze_manager = GazeDataManager(args.acquisition_dir, config_file_path, filter_gaze_data=False)
    gaze_manager.config_manager["max_gaze_speed"] = 0.5
    gaze_manager._filter_gaze_speed()

    # defining the relevant interval of gaze points
    acquisition_offset = 0.1
    start_gaze_i = int(acquisition_offset * gaze_manager.n_gaze_acquisitions)
    stop_gaze_i = int((1 - acquisition_offset) * gaze_manager.n_gaze_acquisitions)
    n_gaze_points = len(range(start_gaze_i, stop_gaze_i))

    # getting a frame from the full screen recording (for displaying gaze points)
    video_manager = VideoManager(gaze_manager.folder_manager["screen_rec_video"])
    screen_cap = video_manager[video_manager.frame_count // 2]

    # defining the visual target positions
    display_targets = []
    config = gaze_manager.config_manager
    display_targets.append((config["display_x"], config["display_y"]))
    display_targets.append((config["display_x"] + config["display_width"], config["display_y"]))
    display_targets.append((config["display_x"], config["display_y"] + config["display_height"]))
    display_targets.append((config["display_x"] + config["display_width"], config["display_y"] + config["display_height"]))

    # defining avg px distances between gaze points and their nearest target
    avg_disctance_px = 0
    avg_x_distance_px = 0
    avg_y_distance_px = 0

    # going through the relevant gaze points (delay from start and finish)
    for gaze_i in range(start_gaze_i, stop_gaze_i):

        # getting the gaze point position (pixels)
        x_coord = int(round(gaze_manager.gaze_data.loc[gaze_i, "X"] * config["screen_width"]))
        y_coord = int(round(gaze_manager.gaze_data.loc[gaze_i, "Y"] * config["screen_height"]))

        # identifying the closest target to the point
        min_distance_x = 0
        min_distance_y = 0
        min_distance = config["screen_width"]
        for target_i in range(len(display_targets)):

            x_distance = x_coord - display_targets[target_i][0]
            y_distance = y_coord - display_targets[target_i][1]
            direct_distance = math.sqrt(x_distance**2 + y_distance**2)
            
            if direct_distance < min_distance :
                min_distance_x = x_distance
                min_distance_y = y_distance
                min_distance = direct_distance

        # accumulating the error measurements
        avg_disctance_px += min_distance
        avg_x_distance_px += min_distance_x
        avg_y_distance_px += min_distance_y

        # adding the gaze point to the frame
        cv2.circle(screen_cap, (x_coord, y_coord), 3, (255, 0, 0), 1)

    # calculatingthe avg errors values
    avg_disctance_px /= n_gaze_points
    avg_x_distance_px /= n_gaze_points
    avg_y_distance_px /= n_gaze_points
    width_error = (avg_disctance_px/config["screen_width"]) * 100
    height_error = (avg_disctance_px/config["screen_height"]) * 100

    # displaying measurement values   
    print(f"Average gaze error (pixels) : {avg_disctance_px} px")
    print(f"Average gaze x component error (pixels) : {avg_x_distance_px} px")
    print(f"Average gaze y component error (pixels) : {avg_y_distance_px} px")
    print(f"Error percentage for screen width : {width_error} %")
    print(f"Error percentage for screen height : {height_error} %")

    # displaying gaze points
    cv2.imshow("Gaze points", screen_cap)
    cv2.waitKey(0)