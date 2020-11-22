import time
from enum import Enum

import cv2
import imutils
import numpy as np
import pyrealsense2 as rs

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


    def __init__(self, video_file_path, video_source_type=VideoSource.VIDEO_FILE):
        
        '''
        Parameters
        ----------
        video_file_path (str) : path to the video file to be loaded
        video_source_type (VideoSource - enum) : source of the target video 
        '''

        self.video_source_type = video_source_type
        self.video_file_path = video_file_path

        self.frame_count = None
        self.video_source = None

        # defining the display frame rates
        if self.video_source_type == VideoSource.VIDEO_FILE: 
            self.display_period = 1 / self.default_display_fps
        elif self.video_source_type == VideoSource.REAL_SENS_BAG: 
            self.display_period = 1 / self.realsens_color_fps

        # initialization
        self.get_video_source()
        self.count_video_frames()


    def __del__(self):

        if self.video_source_type == VideoSource.VIDEO_FILE:
            try : self.video_source.release()
            except : pass


    def get_video_source(self):

        '''
        Gets the appropriate frame source, depends on the "video_source" constructor param
            VideoSource.VIDEO_FILE : (cv2.VideoCapture)
            VideoSource.REAL_SENS_BAG : (pyrealsense2.pipeline)
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


    def count_video_frames(self):

        '''
        Counts the frames in the provided vide source

        Returns
        -------
        (int) : number of frames in the video
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
                        self.video_source = self.get_video_source()
                        break
                
        return self.frame_count


    def __iter__(self):
        self.get_video_source()
        return self


    def __next__(self):
    
        ''' 
        Returns the next frame from the video source (in sequential order)
        
        Returns
        -------
        VideoSource.VIDEO_FILE : tuple (frame (np.array), None)
        VideoSource.REAL_SENS_BAG : tuple(color frame (pyrealsense2.video_frame), depth frame (pyrealsense2.depth_frame))
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
        key (int) : clarius acquisition index

        Returns
        -------
        (np.array) : US frame
        '''

        indexed_frame = None

        if isinstance(key, int):

            if self.video_source_type == VideoSource.VIDEO_FILE:     
                
                if key < self.frame_count and key >= 0 :
                    self.video_source.set(cv2.CAP_PROP_POS_FRAMES, key)
                    indexed_frame = self.video_source.read()[1]

                # index out of range
                else:
                    raise ValueError("Index is out of range")    

            # indexing can only be done on video files
            else:
                raise ValueError("Indexing is only available for (VideoSource.VIDEO_FILE) sources")

        # making sure the key is a num
        else:
            raise ValueError("Index key has to be an int")

        return indexed_frame


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
        self.get_video_source()
        cv2.destroyAllWindows()