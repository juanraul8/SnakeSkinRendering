# A Biologically-Inspired Appearance Model for Snake Skin: script to replicate the renders of Figure 6.
# Example command: python ./scripts/ablation_studies.py -width 1280 -height 720 -spp 1024

import os
import sys
from pathlib import Path
import argparse
import itertools

class SnakeSkinUtils:
    def __init__(self, args):
        # Render configuration
        self.width = args.width
        self.height = args.height
        self.n_threads = args.threads
        self.spp = args.spp

        # General configuration
        self.verbose = args.verbose
        self.output_folder = args.output_folder

    def createRenders(self, scene_file = "./scenes/furball/furball_ablation.xml", output_file = "", mitsuba_variable = "material", mitsuba_value = "diffuse"):
        # Render scene
        command = "mitsuba {0} -o {1} -p {2} -Dspp={3} -Dwidth={4} -Dheigth={5} -D{6}={7}".format(scene_file, output_file, self.n_threads, self.spp, \
                  self.width, self.height, mitsuba_variable, mitsuba_value)

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
parser = argparse.ArgumentParser(description="This script will perform the ablation studies of the snake skin BSDF")
parser.add_argument("--output_folder", "-of", type=str, default="./scenes/fig6/renders", help="output folder of the renders")
parser.add_argument("-spp", "--spp", type=int,default = 64, help="set the number of samples per pixel")
parser.add_argument("-p", "--threads", type=int,default = 20, help="set the number of threads to be used")
parser.add_argument("-width", "--width", type=int, default=256, help="width of the image")
parser.add_argument("-height", "--height", type=int, default=256, help="height of the image")
parser.add_argument('-v', "--verbose", action='store_false', help="enable verbose mode")           

args = parser.parse_args() 

snake_skin_utils = SnakeSkinUtils(args = args)

verbose = args.verbose
output_folder = args.output_folder

# ------------------------- Appearance range study Snake Skin BSDF (Fig 5.) --------------------------
material_names = ["diffuse", "diffuse_bump", "thin_film_bump", "multilayered"]
shape_names = ["sphere", "torus", "snake"]
mitsuba_variable = "material"


scene_files = {
  "sphere": "./scenes/fig6/fig6_sphere.xml",
  "torus": "./scenes/fig6/fig6_torus.xml",
  "snake": "./scenes/fig6/fig6_snake.xml"
}

# Create renders folder
if not os.path.exists(output_folder):
    os.makedirs(output_folder)

for i, shape_name in enumerate(shape_names):

    scene_file = scene_files[shape_name]

    for j, material_name in enumerate(material_names):

        if verbose:
            print("Creating renders for experiment Fig 6 (material = {0}, shape = {1})".format(material_name, shape_name))

        output_file = os.path.join(output_folder, "fig6_{0}_{1}.exr".format(material_name, shape_name))

        snake_skin_utils.createRenders(scene_file = scene_file, output_file = output_file, mitsuba_variable = mitsuba_variable, mitsuba_value = material_name)