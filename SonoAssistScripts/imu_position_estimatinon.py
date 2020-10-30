import argparse
import numpy as np
from matplotlib import animation
from matplotlib import pyplot as plt
import mpl_toolkits.mplot3d.axes3d as p3

from sonopy.clarius import ClariusDataManager

class DisplayFlags:
    count = 0
    update_points = True

if __name__ == "__main__":

    # parsing script arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("acquisition_dir", help="Directory containing the acquisition files")
    args = parser.parse_args()

    # defining the coordinate containers
    x_positions = []
    y_positions = []
    z_positions = []
    
    # loading clarius data + down sampling position points
    clarius_manager = ClariusDataManager(args.acquisition_dir)
    for i in range(0, clarius_manager.n_acquisitions, clarius_manager.acquisition_rate//2):
        x_positions.append(clarius_manager.imu_positions[i][2]*-1)
        y_positions.append(clarius_manager.imu_positions[i][1])
        z_positions.append(clarius_manager.imu_positions[i][0])
    n_positions = len(x_positions)

    # defining the 3 plot for display
    fig = plt.figure()
    ax = p3.Axes3D(fig)
    ax.set_xlabel('X')
    ax.set_ylabel('Y')
    ax.set_zlabel("Z")
    txt = fig.suptitle('')

    # defining the points for the probe and area limits
    points_x = np.array([z_positions[0]]*n_positions)
    points_y = np.array([y_positions[0]]*n_positions)
    points_z = np.array([x_positions[0]]*n_positions) * -1
    points, = ax.plot(points_x, points_y, points_z, '*', color="red")
    corner_points, = ax.plot([min(x_positions), max(x_positions)], 
                             [min(y_positions), max(y_positions)], 
                             [min(z_positions), max(z_positions)], 'o', color="blue")

    # checking movement ranges
    print("x range : {}".format(max(x_positions)- min(x_positions)))
    print("y range : {}".format(max(y_positions)- min(y_positions)))
    print("z range : {}".format(max(z_positions)- min(z_positions)))
    
    # defining the point position update function
    def update_points(frame_i):

        # draw condition
        if frame_i == 0:
            if DisplayFlags.count < 2:
                DisplayFlags.update_points = True
                DisplayFlags.count += 1
            else: DisplayFlags.update_points = False

        # updating the positions and display text
        if DisplayFlags.update_points == True: 

            txt.set_text('point : {:d}'.format(frame_i))

            points_x[frame_i] = x_positions[frame_i]
            points_y[frame_i] = y_positions[frame_i]
            points_z[frame_i] = z_positions[frame_i]
            points.set_data(points_x, points_y)
            points.set_3d_properties(points_z, 'z')

        return points, txt

    # defining and launching the animation
    ani=animation.FuncAnimation(fig, update_points, frames=n_positions)
    plt.show()