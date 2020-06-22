import argparse
from sonopy.tracking import SonoTracker

# parsing script arguments
parser = argparse.ArgumentParser()
parser.add_argument("config_path", help="path to the .json config file")
args = parser.parse_args()

if __name__ == "__main__":
    tracker = SonoTracker(args.config_path, debug=True)
    tracker.launch_tracking()