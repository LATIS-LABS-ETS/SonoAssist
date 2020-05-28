# -----------------------------------------------------------------------------
# Copyright (c) 2016 Nicolas P. Rougier. All rights reserved.
# Distributed under the (new) BSD License.
# -----------------------------------------------------------------------------
import sys
import math
import ctypes
import numpy as np
import OpenGL.GL as gl
import OpenGL.GLUT as glut

import time
import redis

vertex_code = """
uniform mat4   u_model;         // Model matrix
uniform mat4   u_view;          // View matrix
uniform mat4   u_projection;    // Projection matrix
uniform vec4   u_color;         // Global color

attribute vec4 a_color;         // Vertex color
attribute vec3 a_position;      // Vertex position

varying vec4   v_color;         // Interpolated fragment color (out)
void main()
{
    v_color = u_color * a_color;
    gl_Position = u_projection * u_view * u_model * vec4(a_position,1.0);
}
"""

fragment_code = """
varying vec4 v_color;         // Interpolated fragment color (in)
void main()
{
    gl_FragColor = v_color;
}
"""

class GyroRedisModel:

    ''' Allows to pull orientation data from redis '''

    def __init__(self):
        
        self.z_axis = 0
        self.y_axis = 0
        self.x_axis = 0

        # connecting to redis and waiting for data
        self.r_connection = redis.StrictRedis(decode_responses=True)
        while not self.r_connection.exists("gyro_data") == 1:
            time.sleep(0.1)

    def get_orientation_data(self):

        ''' returns orientation data in order : [heading, pitch, roll, yaw] '''

        orientation_data = None

        # extracting orientations from the string
        if self.r_connection.llen("gyro_data") > 1:
            data_str = self.r_connection.lpop("gyro_data")
            if not data_str == "":
                orientation_data = [float(element) for element in data_str.strip().split(",")]

        return orientation_data
        
    def apply_model_rotations(self, model):
    
        ''' updates and applies proper axis rotation angles '''

        # update the rotation angles if data was pulled
        orientation_data = self.get_orientation_data()
        if orientation_data is not None:
            self.z_axis = orientation_data[3]
            self.y_axis = orientation_data[2]
            self.x_axis = orientation_data[1]

        # applying the current rotation angles
        rotate(model, self.z_axis, 1, 0, 0)
        rotate(model, self.y_axis, 0, 0, 1)        
        rotate(model, self.x_axis, 0, 1, 0)

        time.sleep(0.1)

        return model


def rotate(M, angle, x, y, z, point=None):
    angle = math.pi * angle / 180
    c, s = math.cos(angle), math.sin(angle)
    n = math.sqrt(x * x + y * y + z * z)
    x, y, z = x/n, y/n, z/n
    cx, cy, cz = (1 - c) * x, (1 - c) * y, (1 - c) * z
    R = np.array([[cx * x + c, cy * x - z * s, cz * x + y * s, 0],
                  [cx * y + z * s, cy * y + c, cz * y - x * s, 0],
                  [cx * z - y * s, cy * z + x * s, cz * z + c, 0],
                  [0, 0, 0, 1]], dtype=M.dtype).T
    M[...] = np.dot(M, R)
    return M

def translate(M, x, y=None, z=None):
    y = x if y is None else y
    z = x if z is None else z
    T = np.array([[1.0, 0.0, 0.0, x],
                  [0.0, 1.0, 0.0, y],
                  [0.0, 0.0, 1.0, z],
                  [0.0, 0.0, 0.0, 1.0]], dtype=M.dtype).T
    M[...] = np.dot(M, T)
    return M

def frustum(left, right, bottom, top, znear, zfar):
    M = np.zeros((4, 4), dtype=np.float32)
    M[0, 0] = +2.0 * znear / (right - left)
    M[2, 0] = (right + left) / (right - left)
    M[1, 1] = +2.0 * znear / (top - bottom)
    M[3, 1] = (top + bottom) / (top - bottom)
    M[2, 2] = -(zfar + znear) / (zfar - znear)
    M[3, 2] = -2.0 * znear * zfar / (zfar - znear)
    M[2, 3] = -1.0
    return M

def perspective(fovy, aspect, znear, zfar):
    h = math.tan(fovy / 360.0 * math.pi) * znear
    w = h * aspect
    return frustum(-w, w, -h, h, znear, zfar)

def display():
    
    global gyro_model, x_axis, y_axis, z_axis

    gl.glClear(gl.GL_COLOR_BUFFER_BIT | gl.GL_DEPTH_BUFFER_BIT)

    # Outlined cube
    gl.glDisable(gl.GL_POLYGON_OFFSET_FILL)
    gl.glEnable(gl.GL_BLEND)
    gl.glDepthMask(gl.GL_FALSE)
    gl.glUniform4f(gpu["uniform"]["u_color"], 0, 0, 0, .5)
    gl.glBindBuffer(gl.GL_ELEMENT_ARRAY_BUFFER, gpu["buffer"]["outline"])
    gl.glDrawElements(gl.GL_LINES, len(o_indices), gl.GL_UNSIGNED_INT, None)

    # applying rotations (3 axis)
    model = np.eye(4, dtype=np.float32)
    model = gyro_model.apply_model_rotations(model)
    
    gl.glUniformMatrix4fv(gpu["uniform"]["u_model"], 1, False, model)
    glut.glutSwapBuffers()
    
def reshape(width,height):
    gl.glViewport(0, 0, width, height)
    projection = perspective(45.0, width / float(height), 2.0, 100.0)
    gl.glUniformMatrix4fv(gpu["uniform"]["u_projection"], 1, False, projection)
    
def keyboard( key, x, y ):
    if key == b'\033':  sys.exit( )
        
def timer(fps):
    glut.glutTimerFunc(1000//fps, timer, fps)
    glut.glutPostRedisplay()


# GLUT init
# --------------------------------------
glut.glutInit()
glut.glutInitDisplayMode(glut.GLUT_DOUBLE | glut.GLUT_RGBA | glut.GLUT_DEPTH)
glut.glutCreateWindow('Sensor Orientation')
glut.glutReshapeWindow(512, 512)
glut.glutReshapeFunc(reshape)
glut.glutDisplayFunc(display)
glut.glutKeyboardFunc(keyboard)
glut.glutTimerFunc(1000//60, timer, 60)

# Build cube
# --------------------------------------
vertices = np.zeros(8, [("a_position", np.float32, 3),
                        ("a_color", np.float32, 4)])
vertices["a_position"] = [[ 1, 1, 1], [-1, 1, 1], [-1,-1, 1], [ 1,-1, 1],
                          [ 1,-1,-1], [ 1, 1,-1], [-1, 1,-1], [-1,-1,-1]]
vertices["a_color"]    = [[0, 1, 1, 1], [0, 0, 1, 1], [0, 0, 0, 1], [0, 1, 0, 1],
                          [1, 1, 0, 1], [1, 1, 1, 1], [1, 0, 1, 1], [1, 0, 0, 1]]
f_indices = np.array([0,1,2, 0,2,3,  0,3,4, 0,4,5,  0,5,6, 0,6,1,
                      1,6,7, 1,7,2,  7,4,3, 7,3,2,  4,7,6, 4,6,5], dtype=np.uint32)
o_indices = np.array([0,1, 1,2, 2,3, 3,0, 4,7, 7,6,
                      6,5, 5,4, 0,5, 1,6, 2,7, 3,4], dtype=np.uint32)

# Build & activate program
# --------------------------------------
program  = gl.glCreateProgram()
vertex = gl.glCreateShader(gl.GL_VERTEX_SHADER)
fragment = gl.glCreateShader(gl.GL_FRAGMENT_SHADER)
gl.glShaderSource(vertex, vertex_code)
gl.glCompileShader(vertex)
gl.glAttachShader(program, vertex)
gl.glShaderSource(fragment, fragment_code)
gl.glCompileShader(fragment)
gl.glAttachShader(program, fragment)
gl.glLinkProgram(program)
gl.glDetachShader(program, vertex)
gl.glDetachShader(program, fragment)
gl.glUseProgram(program)


# Build GPU objects
# --------------------------------------
gpu = { "buffer" : {}, "uniform" : {} }

gpu["buffer"]["vertices"] = gl.glGenBuffers(1)
gl.glBindBuffer(gl.GL_ARRAY_BUFFER, gpu["buffer"]["vertices"])
gl.glBufferData(gl.GL_ARRAY_BUFFER, vertices.nbytes, vertices, gl.GL_DYNAMIC_DRAW)
stride = vertices.strides[0]

offset = ctypes.c_void_p(0)
loc = gl.glGetAttribLocation(program, "a_position")
gl.glEnableVertexAttribArray(loc)
gl.glVertexAttribPointer(loc, 3, gl.GL_FLOAT, False, stride, offset)

offset = ctypes.c_void_p(vertices.dtype["a_position"].itemsize)
loc = gl.glGetAttribLocation(program, "a_color")
gl.glEnableVertexAttribArray(loc)
gl.glVertexAttribPointer(loc, 4, gl.GL_FLOAT, False, stride, offset)

gpu["buffer"]["filled"] = gl.glGenBuffers(1)
gl.glBindBuffer(gl.GL_ELEMENT_ARRAY_BUFFER, gpu["buffer"]["filled"])
gl.glBufferData(gl.GL_ELEMENT_ARRAY_BUFFER, f_indices.nbytes, f_indices, gl.GL_STATIC_DRAW)

gpu["buffer"]["outline"] = gl.glGenBuffers(1)
gl.glBindBuffer(gl.GL_ELEMENT_ARRAY_BUFFER, gpu["buffer"]["outline"])
gl.glBufferData(gl.GL_ELEMENT_ARRAY_BUFFER, o_indices.nbytes, o_indices, gl.GL_STATIC_DRAW)

# Bind uniforms
# --------------------------------------
gpu["uniform"]["u_model"] = gl.glGetUniformLocation(program, "u_model")
gl.glUniformMatrix4fv(gpu["uniform"]["u_model"], 1, False, np.eye(4))

gpu["uniform"]["u_view"] = gl.glGetUniformLocation(program, "u_view")
view = translate(np.eye(4), 0, 0, -5)
gl.glUniformMatrix4fv(gpu["uniform"]["u_view"], 1, False, view)

gpu["uniform"]["u_projection"] = gl.glGetUniformLocation(program, "u_projection")
gl.glUniformMatrix4fv(gpu["uniform"]["u_projection"], 1, False, np.eye(4))

gpu["uniform"]["u_color"] = gl.glGetUniformLocation(program, "u_color")
gl.glUniform4f(gpu["uniform"]["u_color"], 1, 1, 1, 1)

gyro_model = GyroRedisModel()

# Enter mainloop
# --------------------------------------
gl.glClearColor(1,1,1,1)
gl.glPolygonOffset(1, 1)
gl.glEnable(gl.GL_LINE_SMOOTH)
gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
gl.glHint(gl.GL_LINE_SMOOTH_HINT, gl.GL_NICEST)
gl.glLineWidth(1.0)
glut.glutMainLoop()
