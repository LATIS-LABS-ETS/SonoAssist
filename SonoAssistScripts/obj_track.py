import argparse

from sonopy.tracking import SonoTracker
from sonopy.file_management import SonoFolderManager

# parsing script arguments
parser = argparse.ArgumentParser()
parser.add_argument("config_path", help="path to the .json config file")
args = parser.parse_args()

if __name__ == "__main__":

    folder_manager = SonoFolderManager(args.config_path)
    tracker = SonoTracker(args.config_path, folder_manager.get_video_file_path())
    real_world_positions = tracker.launch_tracking()

    print(real_world_positions)