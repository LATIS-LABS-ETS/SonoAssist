import numpy as np
import pandas as pd

from sonopy.video import VideoManager, VideoSource
from sonopy.file_management import SonoFolderManager


class ClariusDataManager():

    ''' Manages IMU data and US images from the clarius probe '''

    def __init__(self, acquisition_dir_path):

        ''' 
        Parameters
        ----------
        acquisition_dir_path (str) : path to the acquisition directory
        '''

        # loading clarius data from the acquisition folder
        self.folder_manager = SonoFolderManager(acquisition_dir_path)
        self.clarius_df = self.folder_manager.load_clarius_data()
        self.clarius_video = VideoManager(self.folder_manager["clarius_video"])

        # converting timestamps from str to numeric (clarius csv data)
        self.clarius_df["Reception OS time"] = pd.to_numeric(self.clarius_df["Reception OS time"], errors="coerce")
        self.clarius_df["Display OS time"] = pd.to_numeric(self.clarius_df["Display OS time"], errors="coerce")
        self.clarius_df["Onboard time"] = pd.to_numeric(self.clarius_df["Onboard time"], errors="coerce")

        # removing / averaging out extra IMU acquisitions
        self.avg_imu_data()
        self.n_acquisitions = len(self.clarius_df.index)
        
        
    def get_nearest_index(self, target_time, time_col_name="Display OS time"):

        ''' 
        Returns the index associated to the acquisition closest to the provided timestamp
        This function works with the "Display OS time" timestamp (for pairing acquisitions with gaze data)

        Parameters
        ----------
        target_time (int) : timestamp (us)
        time_col_name (str) : identifier for the column to use for the timestamp calculations

        Returns
        -------
        (int) : index for the nearest video frame and IMU acquisitions
        '''

        # getting the nearest index before the specified time
        nearest_index = None
        before_index = self.clarius_df.index[self.clarius_df[time_col_name] <= target_time][-1]
        before_time = self.clarius_df.loc[before_index, time_col_name]
        
        # choosing between the indexes before and after the specified time

        if before_time == target_time:
            nearest_index = before_index    
        
        else :

            after_index = before_index + 1
            after_time = self.clarius_df.loc[after_index, time_col_name]
            
            if (target_time - before_time) <= (after_time - target_time):
                nearest_index = before_index
            else:
                nearest_index = after_index
        
        return nearest_index
        

    def avg_imu_data(self):

        ''' Averages IMU acquisitions to get one per US image '''

        # defining data containers
        gx_vals = []
        gy_vals = []
        gz_vals = []
        ax_vals = []
        ay_vals = []
        az_vals = []

        # getting useful indexes
        img_indexes = self.clarius_df.index[self.clarius_df['Reception OS time'].notnull()].tolist()
        drop_indexes = self.clarius_df.index[self.clarius_df['Reception OS time'].isnull()]

        # going through every US image receptions
        for i in range(len(img_indexes)-1):

            start_index = img_indexes[i]
            stop_index = img_indexes[i+1]
            n_acquisitions = stop_index - start_index
            if n_acquisitions > 1:

                # collecting IMU acquisitions associated to a singe US image
                for x in range(start_index, stop_index, 1):
                    gx_vals.append(self.clarius_df.loc[x, "gx"])
                    gy_vals.append(self.clarius_df.loc[x, "gy"])
                    gz_vals.append(self.clarius_df.loc[x, "gz"])
                    ax_vals.append(self.clarius_df.loc[x, "ax"])
                    ay_vals.append(self.clarius_df.loc[x, "ay"])
                    az_vals.append(self.clarius_df.loc[x, "ax"])

                # calculating avg values, overiding the start acquisition
                self.clarius_df.loc[start_index, "gx"] = sum(gx_vals) / n_acquisitions
                self.clarius_df.loc[start_index, "gy"] = sum(gy_vals) / n_acquisitions
                self.clarius_df.loc[start_index, "gz"] = sum(gz_vals) / n_acquisitions
                self.clarius_df.loc[start_index, "ax"] = sum(ax_vals) / n_acquisitions
                self.clarius_df.loc[start_index, "ay"] = sum(ay_vals) / n_acquisitions
                self.clarius_df.loc[start_index, "az"] = sum(az_vals) / n_acquisitions 

                # clearing the data containers
                gx_vals.clear()
                gy_vals.clear()
                gz_vals.clear()
                ax_vals.clear()
                ay_vals.clear()
                az_vals.clear()

        # removing all non-first acquisitions
        self.clarius_df.drop(drop_indexes, inplace=True)
        self.clarius_df.reset_index(drop=True, inplace=True)