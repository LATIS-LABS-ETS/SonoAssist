'''
Implementation of the temporal calibration procedure detailed in the
((Stradx: real-time acquisition and visualisation of freehand 3D ultrasound) article

Hold still period : 10 seconds

Usage : 

    python3 temporal_calibration.py (path to the acquisition folder)

'''

import cv2
import argparse
import numpy as np

from sonopy.clarius import ClariusDataManager
from sonopy.video import VideoManager, VideoSource
from sonopy.file_management import SonoFolderManager


def img_diff(img1, img2):

    '''
    Computes the image difference measure as dexcribed in the 
    (Stradx: real-time acquisition and visualisation of freehand 3D ultrasound) article
    
    Parameters
    ----------
    img1 (np.array): first image
    img2 (np.array): follow up image of (img1) in the video

    Returns
    -------
    (float) : image difference measure
    '''

    sum_spacing = 10

    # defining the sum containers
    row_sums_1 = []
    row_sums_2 = []
    col_sums_1 = []
    col_sums_2 = []

    # getting image dimensions
    (img_h, img_w, _) = img1.shape

    # converting input images to grayscale
    img1 = cv2.cvtColor(img1, cv2.COLOR_BGR2GRAY)
    img2 = cv2.cvtColor(img2, cv2.COLOR_BGR2GRAY)

    # collecting sum of rows
    for r_i in range(0, img_h, sum_spacing):
        row_sums_1.append(np.sum(img1[r_i, : ]))
        row_sums_2.append(np.sum(img2[r_i, : ]))
    row_sums_1 = np.array(row_sums_1, dtype=np.int64)
    row_sums_2 = np.array(row_sums_2, dtype=np.int64)

    # collecting sum of collumns
    for c_i in range(0, img_w, sum_spacing):
        col_sums_1.append(np.sum(img1[ : , c_i]))
        col_sums_2.append(np.sum(img2[ : , c_i]))
    col_sums_1 = np.array(col_sums_1, dtype=np.int64)
    col_sums_2 = np.array(col_sums_2, dtype=np.int64)

    # accumulating the differences to get the diff measure
    row_diffs = np.abs(row_sums_1 - row_sums_2)
    col_diffs = np.abs(col_sums_1 - col_sums_2)
    return np.sum(row_diffs) + np.sum(col_diffs)


def acc_diff(acq1, acq2):

    '''
    Computes the difference (%) in the norm of two acceleration vectors

    Parameters
    ----------
    acq1 : (pandas.Series) containting the (ax, ay, az) fields for the first acquisition
    acq2 : (pandas.Series) containting the (ax, ay, az) fields for the second acquisition

    Returns
    -------
    (float) : difference between norms (0 - 1)
    '''

    norm1  = np.sqrt(acq1["ax"]**2 + acq1["ay"]**2 + acq1["az"]**2)
    norm2  = np.sqrt(acq2["ax"]**2 + acq2["ay"]**2 + acq2["az"]**2)
    return np.abs(norm2-norm1) / norm1


def calculate_motion_offset(acquisition_dir):

    ''' 
    Calculates the time difference in (us) between the starting point of the jerk motion for the 2 streams : 
        - clarius ultrasound images
        - clarius IMU data

    Parameters
    ----------
    acquisition_dir (str) : path to the target acquisition directory

    Returns
    -------
    tuple()
        1 (float) : time offset (us) between the IMU data stream and the US image stream
        2 (int) : motion start index for clarius IMU diff measures
        3 (int) : motion start index for clarius image diff measures
        4 (list(float)) : difference of acceleration norm between all IMU acquisitions (in %)
        5 (list(float)) : difference score between all US images 
    ''' 

    # timestamp source to be used for calculations
    target_time_stamp = "Onboard time"
    # time before the jerk motion (ns)
    time_before_motion = 10000000000
    # image diff value percentage threshold (0 - 1)
    img_diff_tresh_per = 0.1
    # acc diff value percentage threshold (0 - 1)
    acc_diff_tresh_per = 0.1

    # defining data containers
    acc_diff_measures = []
    img_diff_measures = []

    # loading clarius acquisition data
    clarius_data_manager = ClariusDataManager(acquisition_dir)
   
    # collecting image difference measures
    previous_img = None
    for image_i, current_img in enumerate(clarius_data_manager.clarius_video):
        if previous_img is not None:
            img_diff_measures.append(img_diff(current_img[0], previous_img[0]))
        previous_img = current_img

    # collecting acceleration differences
    for acc_i in range(clarius_data_manager.n_acquisitions - 1):
        acc_diff_measures.append(acc_diff(clarius_data_manager.clarius_df.iloc[acc_i], 
                                          clarius_data_manager.clarius_df.iloc[acc_i + 1]))

    # locating motion start time (image data)

    avg_end_time = clarius_data_manager.clarius_df.loc[0, target_time_stamp] + time_before_motion
    avg_end_index = clarius_data_manager.clarius_df.index[clarius_data_manager.clarius_df[target_time_stamp] <= avg_end_time][-1]
    avg_img_diff = sum(img_diff_measures[ : avg_end_index]) / avg_end_index
    img_diff_tresh = avg_img_diff * (1 + img_diff_tresh_per)

    img_motion_start_index = 0
    img_motion_start_time = None
    for img_i in range(avg_end_index + 1, len(img_diff_measures)):
        if img_diff_measures[img_i] > img_diff_tresh:
            img_motion_start_index = img_i
            img_motion_start_time = (clarius_data_manager.clarius_df.loc[img_i, target_time_stamp] + 
                                     clarius_data_manager.clarius_df.loc[img_i + 1, target_time_stamp])/2
            break

    # locating motion start time (acc data)
    acc_motion_start_index = 0
    acc_motion_start_time = None
    for acc_i in range(len(acc_diff_measures)):
        if acc_diff_measures[acc_i] > acc_diff_tresh_per:
            acc_motion_start_index = acc_i
            acc_motion_start_time = (clarius_data_manager.clarius_df.loc[acc_i, target_time_stamp] + 
                                     clarius_data_manager.clarius_df.loc[acc_i + 1, target_time_stamp])/2
            break

    # calculating the motion start offset (us)
    motion_offset_data = None
    if (img_motion_start_time is not None) and (acc_motion_start_time is not None):
        motion_offset = (np.abs(img_motion_start_time - acc_motion_start_time)) / 10000
        motion_offset_data = (motion_offset, acc_motion_start_index, acc_motion_start_index, acc_diff_measures, img_diff_measures)

    return motion_offset_data


if __name__ == "__main__":

    # parsing input arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Directory containing the acquisition files")
    args = parser.parse_args()
    
    motion_offset_data = calculate_motion_offset(args.acquisition_dir)
    print(f"Time offset (us) between the IMU data stream and the US image stream : {motion_offset_data[0]}")