import numpy as np
import pandas as pd

from sonopy.video import VideoManager, VideoSource
from sonopy.file_management import SonoFolderManager


class ClariusDataManager():

    ''' Manages IMU data and US images from the clarius probe '''

    df_time_fields = ["Reception OS time", "Display OS time", "Onboard time"]

    def __init__(self, acquisition_dir_path, process_data=True):

        ''' 
        Parameters
        ----------
        acquisition_dir_path (str) : path to the acquisition directory
        process_data (bool) : when True, default data processing in the constructor
        '''

        # loading clarius data from the acquisition folder
        self.process_data = process_data
        self.folder_manager = SonoFolderManager(acquisition_dir_path)
        self.clarius_df = self.folder_manager.load_clarius_data()
        self.clarius_video = VideoManager(self.folder_manager["clarius_video"])

        # acquisition count
        self.n_acquisitions = len(self.clarius_df.index)

        # converting timestamps from str to numeric (clarius csv data)
        self.clarius_df["Reception OS time"] = pd.to_numeric(self.clarius_df["Reception OS time"], errors="coerce")
        self.clarius_df["Display OS time"] = pd.to_numeric(self.clarius_df["Display OS time"], errors="coerce")
        self.clarius_df["Onboard time"] = pd.to_numeric(self.clarius_df["Onboard time"], errors="coerce")

        # removing / averaging out extra IMU acquisitions
        if self.process_data :
            self.avg_imu_data()
            self.convert_imu_data()
    

    def avg_imu_data(self):

        ''' Averages IMU acquisitions to get one per US image '''

        # getting useful indexes
        img_indexes = self.clarius_df.index[self.clarius_df['Reception OS time'].notnull()].tolist()
        drop_indexes = self.clarius_df.index[self.clarius_df['Reception OS time'].isnull()]

        # going through the image related indexes
        for i in range(len(img_indexes)-1):

            start_index = img_indexes[i]
            stop_index = img_indexes[i+1]
            n_acquisitions = stop_index - start_index
            if n_acquisitions > 1:

                # collecting IMU acquisitions associated to a singe US image
                avg_container = {}
                for x in range(start_index, stop_index, 1):

                    row_data = self.clarius_df.iloc[x]
                    for field, val in row_data.items():
                        
                        if not field in self.df_time_fields:
                            if not field in avg_container: avg_container[field] = []
                            avg_container[field].append(val)

                # calculating avg values, overiding the start acquisition
                for field in avg_container.keys():
                    self.clarius_df.loc[start_index, field] = sum(avg_container[field]) / n_acquisitions

        # removing all non-first acquisitions
        self.clarius_df.drop(drop_indexes, inplace=True)
        self.clarius_df.reset_index(drop=True, inplace=True)
        self.n_acquisitions = len(self.clarius_df.index)


    def convert_imu_data(self):

        ''' Applies conversions to the acquired IMU data '''

        for row_i in range(self.n_acquisitions):

            # converting the quaternion baesd orientation to euler
            row_data = self.clarius_df.iloc[row_i]
            roll, pitch, yaw = self.quaternion_to_euler_angle(row_data["qw"], row_data["qx"], row_data["qy"], row_data["qz"])
            self.clarius_df.loc[row_i, "roll"] = roll
            self.clarius_df.loc[row_i, "pitch"] = pitch
            self.clarius_df.loc[row_i, "yaw"] = yaw

    
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

        after_index = -1
        before_index = -1
        nearest_index = None

        # getting the before and after indexes
        for i in range(self.n_acquisitions):
            if self.clarius_df.loc[i, time_col_name] > target_time:
                after_index = i
                before_index = i-1
                break
        
        # specified time is before acquisition start
        if (before_index == -1) and (after_index == 0) : 
            nearest_index = 0
        # specified time is after acquisition stop
        elif (before_index == -1) and (after_index == -1):
            nearest_index = self.n_acquisitions - 1

        # specified time is during acquisition
        else:

            after_time = self.clarius_df.loc[after_index, time_col_name]
            before_time = self.clarius_df.loc[before_index, time_col_name]
      
            # choosing between the valid indexes before and after the specified time

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


    def get_clarius_data(self, index, time_span=100000):

        ''' 
        Returns the US frame and the probe motion for the specified index
        Acquisition data must be processed in constructor for this to work (process_data = True)
        time_span = 100000 ~ 2 frames into the future

        Parameters
        ----------
        index (int) : clarius acquisition data index
        time_span (int) : (us) the time span used for the probe displacement calculation

        Returns
        -------
        tuple or None: 
            0 : (np.array) : US frame
            1 : (tuple) : (dax, day, daz) acceleration variation (g)
            2 : (tuple) : (droll, dpitch, dyaw)
        None : if the time span is out of bounds
        '''

        clarius_data = None

        try:

            us_frame = self.clarius_video[index]
            acquisition_data = self.clarius_df.iloc[index]
            
            # calculating the probe motion

            motion_upper_timestamp = acquisition_data["Display OS time"] + time_span
            motion_upper_data = self.clarius_df.iloc[self.get_nearest_index(motion_upper_timestamp)]

            dax = acquisition_data["ax"] - motion_upper_data["ax"]
            day = acquisition_data["ay"] - motion_upper_data["ay"]
            daz = acquisition_data["az"] - motion_upper_data["az"]

            dyaw = acquisition_data["yaw"] - motion_upper_data["yaw"]
            droll = acquisition_data["roll"] - motion_upper_data["roll"]
            dpitch = acquisition_data["pitch"] - motion_upper_data["pitch"]
        
            clarius_data = (us_frame, (dax, day, daz), (droll, dpitch, dyaw))

        except : pass

        return clarius_data

    
    @staticmethod
    def quaternion_to_euler_angle(w, x, y, z):

        ''' 
        quaternion to euler angles conversion function
        X  : Roll , Y : Pitch, Z : Yaw
        source : https://stackoverflow.com/questions/56207448/efficient-quaternions-to-euler-transformation 
        '''

        ysqr = y * y

        t0 = +2.0 * (w * x + y * z)
        t1 = +1.0 - 2.0 * (x * x + ysqr)
        X = np.degrees(np.arctan2(t0, t1))

        t2 = +2.0 * (w * y - z * x)
        t2 = np.where(t2>+1.0,+1.0,t2)

        t2 = np.where(t2<-1.0, -1.0, t2)
        Y = np.degrees(np.arcsin(t2))

        t3 = +2.0 * (w * z + x * y)
        t4 = +1.0 - 2.0 * (ysqr + z * z)
        Z = np.degrees(np.arctan2(t3, t4))

        return X, Y, Z 