import time

import cv2
import imutils
from imutils.video import FPS
from imutils.video import VideoStream

from .config import ConfigurationManager


class SonoTracker:

    ''' 
    Enables the tracking of a custom marker in 3D space 
    The tracker uses the (CSRT) tracker from opencv
    
    User interface : 
        - "s" key : select target object, via bounding rectangle
        - "space bar" : next frame (when detection fails)
    '''

    display_delay = 0.05

    def __init__(self, config_path, debug=False):

        '''
        config_path (str) : path to configuration file
        debug (bool) : when true, tracking is done on video file (defined in config)
        '''

        self.debug = debug
        self.config_manager = ConfigurationManager(config_path)
        self.tracker = cv2.TrackerCSRT_create()


    def launch_tracking(self):

        frame_count = 0
        completion_percentage = 0

        initial_selection = True
        detection_failed = False
        target_object_rect = None

        # defing the video source
        if self.debug : 
            video_source = cv2.VideoCapture(self.config_manager.config_data["debug_video_path"])
        else:
            pass

        # counting frames in vide (for progress display)
        n_total_frames = self.count_video_frames(video_source)

        while(True):

            try :

                # getting the next vide frame
                frame_count += 1
                frame = video_source.read()[1]
                if frame is None :
                    break

                # selection of new target object is required
                if initial_selection or detection_failed :

                    time.sleep(self.display_delay)
                    cv2.imshow("Video", frame)
                    input_key = cv2.waitKey(1) & 0xFF
                    
                    # handling the initial selection
                    if initial_selection and input_key == ord("s"):
                        target_object = cv2.selectROI("Video", frame, fromCenter=False, showCrosshair=True)    
                        if not target_object == (0,0,0,0):
                            self.tracker.init(frame, target_object)
                            initial_selection = False

                    # handling failed detections
                    elif detection_failed:
                        target_object = cv2.selectROI("Video", frame, fromCenter=False, showCrosshair=True)
                        if not target_object == (0,0,0,0):
                            self.tracker.init(frame, target_object)
                            detection_failed = False

                # performing the tracking on the current frame
                else :

                    (success, bounding_box) = self.tracker.update(frame)
                    detection_failed = not success
                    
                    if success :
                        
                        (x, y, w, h) = [int(v) for v in bounding_box]
                        
                        # updating the display (in debug mode)
                        if self.debug : 
                            
                            frame = cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                            
                            time.sleep(self.display_delay)
                            cv2.imshow("Video", frame)
                            cv2.waitKey(1)
                            

                    if frame_count >= (completion_percentage * n_total_frames):
                        print("completion : ", int(completion_percentage * 100), " %")
                        completion_percentage += 0.1

                # delay to ajust the display
                

            # clean up after failure
            except : 
                video_source.release()
                cv2.destroyAllWindows()

        # clean up
        video_source.release()
        cv2.destroyAllWindows()


    @staticmethod
    def count_video_frames(video_source):

        '''
        video_source (cv2.VideoCapture) source of the video frames
        '''

        frame_count = 0
        while video_source.read()[0]:
            frame_count += 1
        
        video_source.set(cv2.CAP_PROP_POS_FRAMES, 0)

        return frame_count
            











            

