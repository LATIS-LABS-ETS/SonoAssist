import numpy as np
import pandas as pd
from skinematics.imus import analytical

from sonopy.video import VideoManager
from sonopy.file_management import SonoFolderManager


class ClariusDataManager():

    ''' Manages IMU data and US images from the clarius probe '''

    gravity = 9.81
    df_time_fields = ["Reception OS time", "Display OS time", "Onboard time"]

    def __init__(self, acquisition_dir_path, imu_available=True):

        ''' 
        Parameters
        ----------
        acquisition_dir_path (str) : path to the acquisition directory
        imu_available (bool) : when True, collected IMU data along side the probe images will be processed
        '''

        # loading clarius data from the acquisition folder
        self.imu_available = imu_available
        self.folder_manager = SonoFolderManager(acquisition_dir_path)
        self.clarius_df = self.folder_manager.load_clarius_data()
        self.clarius_video = VideoManager(self.folder_manager["clarius_video"])

        self.n_acquisitions = len(self.clarius_df.index)

        # converting timestamps from str to numeric (clarius csv data)
        self.clarius_df["Reception OS time"] = pd.to_numeric(self.clarius_df["Reception OS time"], errors="coerce")
        self.clarius_df["Display OS time"] = pd.to_numeric(self.clarius_df["Display OS time"], errors="coerce")
        self.clarius_df["Onboard time"] = pd.to_numeric(self.clarius_df["Onboard time"], errors="coerce")

        # defining position estimation vars (IMU)
        self.imu_positions = []
        self.angular_velocities = []
        self.linear_accelerations = []
        self.acquisition_rate = None

        # averaging and processing the acquired IMU data (if required)
        if self.imu_available :
            
            self._avg_imu_data()
            self._process_imu_data()
            
            # reconstructing the position with an analytical solution (ignoring orientation)
            # assumes a start in a stationary position and no compensation for drift
            _, self.imu_positions, _ = analytical(omega=self.angular_velocities, accMeasured=self.linear_accelerations, 
                                                  rate=self.acquisition_rate)

            
    def _avg_imu_data(self):

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


    def _process_imu_data(self):

        ''' Applies conversions to IMU data and fills containers '''

        # estimating the clarius acquisition rate (Hz)
        acquisition_duration = (self.clarius_df.loc[self.n_acquisitions - 1, "Display OS time"] - self.clarius_df.loc[0, "Display OS time"]) / 1000000
        self.acquisition_rate = int(self.n_acquisitions / acquisition_duration)

        # going through the IMU data
        for row_i in range(self.n_acquisitions):

            # converting the quaternion based orientation to euler angles
            row_data = self.clarius_df.iloc[row_i]
            roll, pitch, yaw = self.quaternion_to_euler_angle(row_data["qw"], row_data["qx"], row_data["qy"], row_data["qz"])
            self.clarius_df.loc[row_i, "roll"] = roll
            self.clarius_df.loc[row_i, "pitch"] = pitch
            self.clarius_df.loc[row_i, "yaw"] = yaw

            # collecting angular and linear accelerations
            self.angular_velocities.append([row_data["gx"], row_data["gy"], row_data["gz"]])
            self.linear_accelerations.append([row_data["ax"]*self.gravity, row_data["ay"]*self.gravity, row_data["az"]*self.gravity])

        self.angular_velocities = np.array(self.angular_velocities)
        self.linear_accelerations = np.array(self.linear_accelerations)

    
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
        IMU data must be available for this to work (imu_available = True)
        *time_span = 100000 ~ 2 frames into the future

        Parameters
        ----------
        index (int) : clarius acquisition data index
        time_span (int) : (us) the time span used for the probe displacement calculation

        Returns
        -------
        when imu_available = True, (np.array) : US frame
        when imu_available = False, tuple or None: 
            0 : (np.array) : US frame
            1 : (tuple) : (dx, dy, dz) position variation (m)
            2 : (tuple) : (droll, dpitch, dyaw) angular variation (degrees)
        None : if the time span is out of bounds
        '''

        clarius_data = None

        try:

            clarius_data = [self.clarius_video[index]]
            
            if self.imu_available:
                
                # getting current imu data (at index)
                cur_pos_data = self.imu_positions[index]
                cur_acq_data = self.clarius_df.iloc[index]
                
                # getting imu data further in time for probe motion calculation
                fut_index = self.get_nearest_index(cur_acq_data["Display OS time"] + time_span)
                fut_pos_data = self.imu_positions[fut_index] 
                fut_acq_data = self.clarius_df.iloc[fut_index]

                # calculating the probe motion
                dx = fut_pos_data[0] - cur_pos_data[0]
                dy = fut_pos_data[1] - cur_pos_data[1]
                dz = fut_pos_data[2] - cur_pos_data[2]
                dyaw = fut_acq_data["yaw"] - cur_acq_data["yaw"]
                droll = fut_acq_data["roll"] - cur_acq_data["roll"]
                dpitch = fut_acq_data["pitch"] - cur_acq_data["pitch"]
            
                clarius_data.append((dx, dy, dz))
                clarius_data.append((droll, dpitch, dyaw))
              
        except : pass

        return tuple(clarius_data)

    
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