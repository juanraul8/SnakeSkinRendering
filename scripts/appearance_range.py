# A Biologically-Inspired Appearance Model for Snake Skin: script to replicate the renders of Figure 5.
# Example command: python ./scripts/appearance_range.py -width 1280 -height 720 -spp 1024

import os
import sys
from pathlib import Path
import argparse
import itertools

class SnakeSkinUtils:
    def __init__(self, args):
        # Render configuration
        self.scene_file = args.scene_file
        self.width = args.width
        self.height = args.height
        self.n_threads = args.threads
        self.spp = args.spp

        # General configuration
        self.verbose = args.verbose
        self.output_folder = args.output_folder

    def createRenders(self, scene_file = "./scenes/furball/furball_ablation.xml", output_file = "", row_variable = "thickness", row_value = "300",\
                      column_variable = "intIOR", column_value = "1.0"):
        # Render scene
        command = "mitsuba {0} -o {1} -p {2} -Dspp={3} -Dwidth={4} -Dheigth={5} -D{6}={7} -D{8}={9}".format(scene_file, output_file, self.n_threads, self.spp, \
                  self.width, self.height, row_variable, row_value, column_variable, column_value)

        # Execute command
        if self.verbose:
            print("Executing command: {0}".format(command))
        
        os.system(command)

        # Tone mapping the image
        command = "mtsutil tonemap {0}".format(output_file)

        if self.verbose:
            print("exr to png command: {0}".format(command))
        
        os.system(command)

# Argument parameters of the program
parser = argparse.ArgumentParser(description="Given a valid scene, this script will perform the appearance range study of two variables of the snake skin BSDF")
parser.add_argument("--scene_file","-i", type=str, default="./scenes/fig5/fig5_snake.xml", help="Scene file to be rendered")
parser.add_argument("--output_folder", "-of", type=str, default="./scenes/fig5/renders", help="output folder of the renders")
parser.add_argument("-spp", "--spp", type=int,default = 64, help="set the number of samples per pixel")
parser.add_argument("-p", "--threads", type=int,default = 20, help="set the number of threads to be used")
parser.add_argument("-width", "--width", type=int, default=256, help="width of the image")
parser.add_argument("-height", "--height", type=int, default=256, help="height of the image")
parser.add_argument('-v', "--verbose", action='store_false', help="enable verbose mode")      

args = parser.parse_args() 

snake_skin_utils = SnakeSkinUtils(args = args)

verbose = args.verbose
scene_file = args.scene_file
output_folder = args.output_folder

# ------------------------- Appearance range study Snake Skin BSDF (Fig 5.) --------------------------
column_name = "thickness"
column_variable = "thickness"
column_values = ["300", "600", "1200"]
row_name = "eta2"
row_variable = "intIOR"
row_values = ["1.0", "2.0"]

# Create renders folder
if not os.path.exists(output_folder):
    os.makedirs(output_folder)

for i, row_value in enumerate(row_values):

    for j, column_value in enumerate(column_values):

        if verbose:
            print("Creating renders for experiment Fig 5 ({0} = {1}, {2} = {3})".format(row_name, row_value, column_name, column_value))

        output_file = os.path.join(output_folder, "fig5_{0}_{1}_{2}_{3}.exr".format(row_name, row_value, column_name, column_value))

        snake_skin_utils.createRenders(scene_file = scene_file, output_file = output_file, row_variable = row_variable, row_value = row_value, \
                                      column_variable = column_variable, column_value = column_value)