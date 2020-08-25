import time

import cv2
import imutils
import numpy as np
import pyrealsense2 as rs

from .video import VideoManager
from .config import ConfigurationManager


class SonoTracker:

    ''' 
    Enables the tracking of a custom marker in 3D space 
    The tracker uses the (CSRT) tracker from opencv
    
    User interface : 
        - "s" key : select target object, via bounding rectangle
        - "space bar" : next frame (when detection fails)
    '''

    display_delay = 0.1

    # defining color filter parameters
    color_filter_margin = 10
    filter_lower_red = np.array([150,50,50]) 
    filter_upper_red = np.array([200,255,255])

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
        
        self.tracker = cv2.TrackerCSRT_create()
        self.video_manager = VideoManager(config_path, video_file_path)

    
    def locate_color_dots(self, frame, bounding_rect):

        ''' 
        Locates the 2D coordinates of the colors dots on the tracked sensor 

        Inputs
        ------
        frame : (numpy.array) color camera frame containing  the tracked sensor
        bounding_rect : (tuple) (x, y, w, h) bounding box around the sensor
        
        Returns
        -------
        (dictionary) : {"color name" : (x px coordinate, y px coordinate)}
        '''

        # getting image and bouding rect info
        frame_max_y = frame.shape[0] - 1
        frame_max_x = frame.shape[1] - 1
        (x, y, w, h) = bounding_rect
        
        # defining the dimensions of the area of interest in the image
        left_x = x - self.color_filter_margin
        if left_x < 0 : left_x = 0
        top_y = y - self.color_filter_margin
        if top_y < 0 : top_y = 0
        right_x = x + w + self.color_filter_margin
        if right_x > frame_max_x : right_x = frame_max_x
        bottom_y = y + h + self.color_filter_margin
        if bottom_y > frame_max_y : bottom_y = frame_max_y
        
        # extracting and converting the interest area
        sub_image = frame[top_y : bottom_y, left_x : right_x]
        sub_image_hsv = cv2.cvtColor(sub_image, cv2.COLOR_BGR2HSV) 
      
        # applying the blue filter
        mask = cv2.inRange(sub_image_hsv, self.filter_lower_red, self.filter_upper_red)
        sub_image = cv2.bitwise_and(sub_image, sub_image, mask=mask)

        # testing
        cv2.imshow("testing", sub_image)
        key = cv2.waitKey()
        cv2.destroyAllWindows()
        
        if key == ord("q"):
            exit()


    def launch_tracking(self):

        ''' Launches object tracking on the frames of the video source '''

        object_positions = np.zeros(shape=(0, 3))

        frame_count = 0
        completion_percentage = 0

        initial_selection = True
        detection_failed = False
        target_object_rect = None

        # checking video information
        if self.video_manager.video_source is None : return
        n_total_frames = self.video_manager.count_video_frames()

        while(True):

            try :

                # getting the next vide frame
                frame_count += 1
                (color_frame, depth_frame) = self.video_manager.get_vide_frame()
                print("color frame : ", color_frame)
                print("depth frame : ", depth_frame)
                if color_frame is None : break

                # selection of new target object is required
                if initial_selection or detection_failed :

                    # display is necessary for object selection
                    time.sleep(self.display_delay)
                    cv2.imshow("Video", color_frame)
                    input_key = cv2.waitKey(1) & 0xFF
                    
                    # handling the initial selection
                    if initial_selection and input_key == ord("s"):

                        target_object = cv2.selectROI("Video", color_frame, fromCenter=False, showCrosshair=True)    
                        if not target_object == (0,0,0,0):
                            self.tracker.init(color_frame, target_object)
                            initial_selection = False
                            cv2.destroyAllWindows()

                    # handling failed detections
                    elif detection_failed:

                        target_object = cv2.selectROI("Video", color_frame, fromCenter=False, showCrosshair=True)
                        if not target_object == (0,0,0,0):
                            self.tracker.init(color_frame, target_object)
                            detection_failed = False
                            cv2.destroyAllWindows()

                # performing the tracking on the current frame
                else :

                    (success, bounding_box) = self.tracker.update(color_frame)
                    detection_failed = not success
                    
                    if success :
                        
                        (x, y, w, h) = [int(v) for v in bounding_box]
                        #dot_coordinates = self.locate_color_dots(color_frame, (x, y, w, h))
                        z = depth_frame.get_distance(x + w/2, y + h/2)
                        object_positions.append([x, y, z])
                        
                        # updating the display (in debug mode)
                        if self.debug : 
                            
                            color_frame = cv2.rectangle(color_frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                            time.sleep(self.display_delay)
                            cv2.imshow("Video", color_frame)
                            cv2.waitKey(1)

                    # displaying processing progress       
                    if frame_count >= (completion_percentage * n_total_frames):
                        print("completion percentage : ", int(completion_percentage * 100), " %")
                        completion_percentage += 0.1

            # stop processing on failure
            except : break
        
        # clean up
        cv2.destroyAllWindows()

        return object_positions