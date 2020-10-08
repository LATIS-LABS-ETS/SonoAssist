'''
Implementation of the temporal calibration procedure detailed in the
((Stradx: real-time acquisition and visualisation of freehand 3D ultrasound) article

Usage : 

    python3 temporal_calibration.py (path to the acquisition folder)

'''

import cv2
import argparse
import numpy as np

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


if __name__ == "__main__":

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Directory containing the acquisition files")
    args = parser.parse_args()

    # loading acquisition data
    folder_manager = SonoFolderManager(args.acquisition_dir)
    clarius_csv_data = folder_manager.load_clarius_data()
    clarius_video_manager = VideoManager(folder_manager.folder_file_paths["clarius_video"])
 
    # defining data containers
    img_diff_measures = []

    # collecting all image difference measures
    previous_img = None
    for image_i, current_img in enumerate(clarius_video_manager):
        
        if previous_img is not None:
            img_diff_measures.append(img_diff(current_img[0], previous_img[0]))
        previous_img = current_img