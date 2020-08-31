import time

import cv2
import imutils
import numpy as np
import pyrealsense2 as rs

from .config import ConfigurationManager


class VideoManager:

    ''' Manages the extraction of frames from video files '''

    frame_timeout_ms = 500

    def __init__(self, config_path, video_file_path):
        
        '''
        Parameters
        ----------
        config_path (str) : path to configuration file
        video_file_path (str) : path to the video file to be loaded
        '''

        # loading configurations
        self.config_path = config_path
        self.config_manager = ConfigurationManager(config_path)
        self.debug = self.config_manager.config_data["debug_mode"]
        
        # defining the video file path
        if self.debug : 
            self.video_file_path = self.config_manager.config_data["debug_video_path"]
        else : self.video_file_path = video_file_path

        # loading the video
        self.n_served_frames = 0
        self.get_video_source()


    def __del__(self):

        if self.debug:
            try : self.video_source.release()
            except : pass


    def get_video_source(self):

        '''
        Generates the appropriate video frames sources (depends on the debugging state)
            video source : (cv2.VideoCapture) when debug mode is on
            video source : (pyrealsense2.pipeline) when debfug is off
        '''

        # resetting the frame counter
        self.n_served_frames = 0

        try :       

            # loading video from regular media file
            if self.debug :
                self.video_source = cv2.VideoCapture(self.video_file_path)

            # loading video from bag file
            else:
                
                self.video_source = rs.pipeline()
                config = rs.config()
                rs.config.enable_device_from_file(config, self.video_file_path, repeat_playback=False)
                
                config.enable_stream(rs.stream.color, 
                    self.config_manager.config_data["realsens_color_width"], self.config_manager.config_data["realsens_color_height"], 
                    rs.format.bgr8, self.config_manager.config_data["realsens_color_fps"])
                config.enable_stream(rs.stream.depth, 
                    self.config_manager.config_data["realsens_depth_width"], self.config_manager.config_data["realsens_depth_height"], 
                    rs.format.bgr8, self.config_manager.config_data["realsens_depth_fps"])
                
                profile = self.video_source.start(config)
                profile.get_device().as_playback().set_real_time(False)
                
        # video source loading failed
        except Exception as e:
            self.video_source = None
            print("Failed to load video source : {}".format(e))


    def count_video_frames(self):

        '''
        Counts the frames in the provided vide source

        Inputs
        ------
        video_source (cv2.VideoCapture) when debug = True, (pyrealsense2.pipeline) when False

        Returns
        -------
        (int) : number of frames 
        '''
        
        frame_count = 0

        # counting frames from normal video media file
        if self.debug :

            while self.video_source.read()[0] : frame_count += 1
            self.video_source.set(cv2.CAP_PROP_POS_FRAMES, 0)

        # couting color frames from a .bag file
        else :

            while True:
                
                try :
                    frames = self.video_source.wait_for_frames(self.frame_timeout_ms)
                    frame_count += 1
                except :
                    video_source = self.get_video_source()
                    break
            
        return frame_count


    def get_vide_frame(self):
    
        ''' 
        Fetches the next sequential frame from the video source
        
        Returns
        -------
        tuple(color frame (np.array) , depth frame (np.array or None)) next frame from the video source
        '''

        frames = [None, None]
        self.n_served_frames += 1

        # getting next frame from normal video media file
        if self.debug : frames[0] = self.video_source.read()[1]
        
        # getting next frame from color video in .bag file
        else :

            try :
                frame_set = self.video_source.wait_for_frames(self.frame_timeout_ms)
                color_frame = frame_set.get_color_frame()
                depth_frame = frame_set.get_depth_frame()
                frames[0] = np.asanyarray(color_frame.get_data())
                frames[1] = np.asanyarray(depth_frame.get_data())
            
            except Exception as e:
                frames = None
                self.n_served_frames -= 1
                print("failed to serve frame, returned None : {}".format(e))

        return tuple(frames)

    
    def play_video(self):

        '''
        Plays the loaded video with the appropriate frame rate
        For bag files, the color video is played
        '''

        # defining the video fps
        if self.debug : video_fps = self.config_manager.config_data["debug_video_fps"]
        else : video_fps = self.config_manager.config_data["realsens_color_fps"]
        sleep_time = 1 / video_fps

        # displaying the video
        video_frame = self.get_vide_frame()
        while video_frame is not None:

            cv2.imshow("Video", video_frame)
            cv2.waitKey(1)

            video_frame = self.get_vide_frame()
            time.sleep(sleep_time)

        # cleaning up
        if self.debug : self.video_source.release()
        self.get_video_source()
        cv2.destroyAllWindows()