import time
from enum import Enum

import cv2
import numpy as np
try: import pyrealsense2 as rs
except: print("Failed to import pyrealsense2")
from sonopy.file_management import SonoFolderManager


class VideoSource(Enum):

    ''' Enum for different video sources '''
    
    VIDEO_FILE = 0
    REAL_SENS_BAG = 1


class VideoManager:

    ''' Manages the extraction of frames from video files '''

    # real sens frame max delay
    frame_timeout_ms = 100

    # defining intel realsens camera params
    realsens_depth_fps = 30
    realsens_depth_width = 1280
    realsens_depth_height = 720
    realsens_color_fps = 30
    realsens_color_width = 1920
    realsens_color_height = 1080

    # default display params
    default_display_fps = 15


    def __init__(self, video_file_path, video_source_type=VideoSource.VIDEO_FILE, acq_folder_path=""):
        
        '''
        Parameters
        ----------
        video_file_path: str
            path to the video file to be loaded
        video_source_type: VideoSource
            source of the target video
        acq_folder_path: str
            path to the acquisition folder (required for indexing frames via timestamps - screen recorder)
        '''

        self.video_file_path = video_file_path
        self.video_source_type = video_source_type

        self.frame_df = None
        self.frame_count = None
        self.video_source = None

        # loading the video frame indexes
        if not acq_folder_path == "":
            self.frame_df = SonoFolderManager(acq_folder_path).load_screen_rec_data()
            self.n_acquisitions = len(self.frame_df.index)

        # defining the display frame rates
        if self.video_source_type == VideoSource.VIDEO_FILE: 
            self.display_period = 1 / self.default_display_fps
        elif self.video_source_type == VideoSource.REAL_SENS_BAG: 
            self.display_period = 1 / self.realsens_color_fps

        # initialization
        self._get_video_source()
        self.count_video_frames()


    def __del__(self):

        if self.video_source_type == VideoSource.VIDEO_FILE:
            try : self.video_source.release()
            except : pass


    def _get_video_source(self):

        '''
        Gets the appropriate frame source, depends on the "video_source_type" parameter
            - When video_source_type = VideoSource.VIDEO_FILE: (cv2.VideoCapture)
            - When video_source_type = VideoSource.REAL_SENS_BAG: (pyrealsense2.pipeline)
        '''

        try :       

            # handling regular media file
            if self.video_source_type == VideoSource.VIDEO_FILE:
                self.video_source = cv2.VideoCapture(self.video_file_path)

            # handling a bag file
            elif self.video_source_type == VideoSource.REAL_SENS_BAG:
                
                self.video_source = rs.pipeline()
                config = rs.config()
                rs.config.enable_device_from_file(config, self.video_file_path, repeat_playback=False)
                
                config.enable_stream(rs.stream.color, self.realsens_color_width, self.realsens_color_height, rs.format.bgr8, self.realsens_color_fps)
                config.enable_stream(rs.stream.depth, self.realsens_depth_width, self.realsens_depth_height, rs.format.z16, self.realsens_depth_fps)
                profile = self.video_source.start(config)
                profile.get_device().as_playback().set_real_time(False)
                
        # video source loading failed
        except Exception as e:
            print(f"Failed to load video source : {e}")
            raise


    def _get_nearest_index(self, target_time, time_col_name="Time (us)"):

        ''' 
        Returns the index associated to the acquisition closest to the provided timestamp
        ** Reminder: requires the (acq_folder_path) to be specified to the constructor

        Parameters
        ----------
        target_time: int
            target timestamp (us)
        time_col_name: str
            identifier for the column to use for the timestamp calculations

        Returns
        -------
        nearest_index: int
            index for the nearest video frame and IMU acquisitions
        '''

        nearest_index = None
        after_index, before_index = -1, -1

        # getting the before and after indexes
        for i in range(self.n_acquisitions):
            if self.frame_df.loc[i, time_col_name] > target_time:
                after_index = i
                before_index = i-1
                break
        
        # handling the edge cases
        if (before_index == -1) and (after_index == 0): nearest_index = 0
        elif (before_index == -1) and (after_index == -1): nearest_index = self.n_acquisitions - 1

        # specified time is during acquisition
        # choosing between the valid indexes before and after the specified time
        else:

            after_time = self.frame_df.loc[after_index, time_col_name]
            before_time = self.frame_df.loc[before_index, time_col_name]

            if before_time == target_time: nearest_index = before_index

            elif (target_time - before_time) <= (after_time - target_time):
                nearest_index = before_index

            else: nearest_index = after_index
        
        return nearest_index


    def __len__(self):

        return self.frame_count


    def __iter__(self):
        
        self._get_video_source()
        return self


    def __next__(self):
    
        ''' 
        Returns the next frame from the video source (in sequential order)
        
        Returns
        -------
        frames: (np.array, None), when VideoSource.VIDEO_FILE
        frames: (pyrealsense2.video_frame, pyrealsense2.depth_frame), when VideoSource.REAL_SENS_BAG
        '''

        frames = [None, None]
        
        # getting next frame from video file
        if self.video_source_type == VideoSource.VIDEO_FILE:
            video_data = self.video_source.read()
            if video_data[0]:
                frames[0] = video_data[1]
            else:
                raise StopIteration
        
        # getting next frames (color and depth) from .bag file
        elif self.video_source_type == VideoSource.REAL_SENS_BAG:

            try :
                frame_set = self.video_source.wait_for_frames(self.frame_timeout_ms)
                frames[0] = frame_set.get_color_frame()
                frames[1] = frame_set.get_depth_frame()
        
            except Exception as e:
                raise StopIteration
        
        return tuple(frames)


    def __getitem__(self, key):
        
        ''' 
        Frame indexing for (VideoSource.VIDEO_FILE) sources 
        
        Parameters
        ----------
        key: int
            target video frame index

        Returns
        -------
        frame: np.array
        '''

        if not isinstance(key, int):
            raise ValueError("Index key has to be an int")

        if not self.video_source_type == VideoSource.VIDEO_FILE:
            raise ValueError("Indexing is only available for (VideoSource.VIDEO_FILE) sources")

        if not (key < self.frame_count and key >= 0):
            raise ValueError("Index is out of range")
                        
        self.video_source.set(cv2.CAP_PROP_POS_FRAMES, key)
        return self.video_source.read()[1]


    def count_video_frames(self):

        '''
        Counts the frames in the provided video source

        Returns
        -------
        frame_count: int
            number of frames in the video
        '''
        
        if self.frame_count is None:
        
            self.frame_count = 0

            # counting frames from normal video media file
            if self.video_source_type == VideoSource.VIDEO_FILE :

                cv_m_version = int(cv2.__version__.split(".")[0])

                # fast frame counting
                try:
                    if cv_m_version >= 3:
                        self.frame_count = int(self.video_source.get(cv2.CAP_PROP_FRAME_COUNT))
                    else:
                        self.frame_count = int(self.video_source.get(cv2.cv.CV_CAP_PROP_FRAME_COUNT))

                # slow frame counting
                except:
                    while self.video_source.read()[0] : self.frame_count += 1
                    self.video_source.set(cv2.CAP_PROP_POS_FRAMES, 0)

            # couting color frames from a .bag file
            elif self.video_source_type == VideoSource.REAL_SENS_BAG:

                while True:
                    try :
                        frames = self.video_source.wait_for_frames(self.frame_timeout_ms)
                        self.frame_count += 1
                    except :
                        self.video_source = self._get_video_source()
                        break
                
        return self.frame_count


    def get_frame_at_time(self, target_time):

        ''' 
        Returns the video frame associated to the acquisition closest to the provided timestamp
        **Reminder: requires the (acq_folder_path) to be specified to the constructor

        Parameters
        ----------
        target_time: int
            target timestamp (us)

        Returns
        -------
        frame: np.array
            video frame nearest to the provided timestamp
        '''

        return self.__getitem__(self._get_nearest_index(target_time))


    def play_video(self):

        '''
        Plays the loaded video with the appropriate frame rate
        For bag files, the color video is played
        '''

        # going through the source frames
        for frames in self:

            # getting the frame for display
            video_frame = None
            if self.video_source_type == VideoSource.VIDEO_FILE:
                video_frame = frames[0]
            elif self.video_source_type == VideoSource.REAL_SENS_BAG:
                video_frame = np.asanyarray(frames[0].get_data())

            # displaying the frame
            cv2.imshow("Video", video_frame)
            cv2.waitKey(1)
            time.sleep(self.display_period)

        # cleaning up
        if self.video_source_type == VideoSource.VIDEO_FILE : 
            self.video_source.release()
        self._get_video_source()
        cv2.destroyAllWindows()