'''
Implementation of the temporal calibration procedure detailed in the
((Stradx: real-time acquisition and visualisation of freehand 3D ultrasound) article

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
    (float) : difference between norms
    '''

    norm1  = np.sqrt(acq1["ax"]**2 + acq1["ay"]**2 + acq1["az"]**2)
    norm2  = np.sqrt(acq2["ax"]**2 + acq2["ay"]**2 + acq2["az"]**2)
    return 100 * (np.abs(norm2-norm1) / norm1)


if __name__ == "__main__":

    # time before the jerk motion (us)
    time_before_motion = 10000000000
    # image diff value percentage threshold (0 - 1)
    img_diff_tresh_per = 0.1
    # acc diff value percentage threshold (0 - 1)
    acc_diff_tresh_per = 0.1

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Directory containing the acquisition files")
    args = parser.parse_args()

    # loading acquisition data
    folder_manager = SonoFolderManager(args.acquisition_dir)
    clarius_data_manager = ClariusDataManager(folder_manager.load_clarius_data())
    clarius_data_manager.avg_imu_data()
    clarius_video_manager = VideoManager(folder_manager.folder_file_paths["clarius_video"])

    # defining data containers
    acc_diff_measures = []
    img_diff_measures = []
    
    # collecting image difference measures
    previous_img = None
    for image_i, current_img in enumerate(clarius_video_manager):
        if previous_img is not None:
            img_diff_measures.append(img_diff(current_img[0], previous_img[0]))
        previous_img = current_img

    # collecting acceleration differences
    for acc_i in range(len(clarius_data_manager.clarius_df.index)-1):
        acc_diff_measures.append(acc_diff(clarius_data_manager.clarius_df.iloc[acc_i], 
                                          clarius_data_manager.clarius_df.iloc[acc_i + 1]))

    # locating the beginning of the motion start (image data)

    avg_end_time = clarius_data_manager.clarius_df.loc[0, "Onboard time"] + time_before_motion
    avg_end_index = clarius_data_manager.clarius_df.index[clarius_data_manager.clarius_df["Onboard time"] <= avg_end_time][-1]
    avg_img_diff = sum(img_diff_measures[ : avg_end_index]) / avg_end_index
    img_diff_tresh = avg_img_diff * (1 + img_diff_tresh_per)

    img_motion_start_time = None
    for img_i in range(avg_end_index + 1, len(img_diff_measures)):
        if img_diff_measures[img_i] > img_diff_tresh:
            img_motion_start_time = ((clarius_data_manager.clarius_df.loc[img_i, "Onboard time"]) + 
                                      clarius_data_manager.clarius_df.loc[img_i+1, "Onboard time"])/2
            break

    